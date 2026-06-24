/*
 * Copyright (c) 2026 0000xFFFF.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <libintl.h> /* must come before libaudcore/i18n.h to prevent macro-poisoning */
#include <math.h>
#include <pthread.h>
#include <sys/stat.h>

#include <QCoreApplication>
#include <QMetaObject>
#include <QMouseEvent>
#include <QPainter>
#include <QTimer>
#include <QWidget>

#include <libaudcore/audstrings.h>
#include <libaudcore/drct.h>
#include <libaudcore/hook.h>
#include <libaudcore/i18n.h>
#include <libaudcore/interface.h>
#include <libaudcore/plugin.h>
#include <libaudcore/runtime.h>
#include <libaudqt/libaudqt.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libswresample/swresample.h>
}

#define NUM_BUCKETS 2000

struct TrackPeaks
{
    int16_t * mins;
    int16_t * maxs;
    int32_t num_buckets;
    int32_t global_peak;
    double duration_sec;

    TrackPeaks()
        : mins(nullptr), maxs(nullptr), num_buckets(0), global_peak(1),
          duration_sec(0)
    {
    }

    ~TrackPeaks()
    {
        delete[] mins;
        delete[] maxs;
    }

    void resize(int32_t n)
    {
        delete[] mins;
        delete[] maxs;
        num_buckets = n;
        mins = new int16_t[n]();
        maxs = new int16_t[n]();
    }
};

static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;
static TrackPeaks * g_peaks = nullptr;  /* envelope currently displayed */
static char * g_current_file = nullptr; /* file the envelope above belongs to */

static char * cache_path_for(const char * filename)
{
    /* simple FNV-1a hash of the path -> cache filename */
    uint64_t h = 1469598103934665603ULL;
    for (int i = 0; filename[i] != '\0'; i++)
    {
        h ^= (unsigned char)filename[i];
        h *= 1099511628211ULL;
    }

    char hex[17];
    snprintf(hex, sizeof hex, "%016llx", (unsigned long long)h);

    const char * user_dir = aud_get_path(AudPath::UserDir);
    int dir_len = strlen(user_dir) + 16;
    char * dir = new char[dir_len];
    snprintf(dir, dir_len, "%s/waveform_cache", user_dir);

    mkdir(dir, 0755);

    int path_len = strlen(dir) + 24;
    char * path = new char[path_len];
    snprintf(path, path_len, "%s/%s.peaks", dir, hex);

    delete[] dir;
    return path;
}

static bool load_cache(const char * path, TrackPeaks * out)
{
    FILE * f = fopen(path, "rb");
    if (!f)
        return false;

    int32_t n = 0;
    bool ok = fread(&n, sizeof n, 1, f) == 1 && n == NUM_BUCKETS;
    if (ok)
    {
        out->resize(n);
        ok = fread(&out->global_peak, sizeof out->global_peak, 1, f) == 1 &&
             fread(&out->duration_sec, sizeof out->duration_sec, 1, f) == 1 &&
             fread(out->mins, sizeof(int16_t), n, f) == (size_t)n &&
             fread(out->maxs, sizeof(int16_t), n, f) == (size_t)n;
    }
    fclose(f);
    return ok;
}

static void save_cache(const char * path, const TrackPeaks * in)
{
    FILE * f = fopen(path, "wb");
    if (!f)
        return;
    int32_t n = in->num_buckets;
    fwrite(&n, sizeof n, 1, f);
    fwrite(&in->global_peak, sizeof in->global_peak, 1, f);
    fwrite(&in->duration_sec, sizeof in->duration_sec, 1, f);
    fwrite(in->mins, sizeof(int16_t), n, f);
    fwrite(in->maxs, sizeof(int16_t), n, f);
    fclose(f);
}

struct PumpContext
{
    int16_t * samples;
    size_t samples_count;
    size_t samples_capacity;
    SwrContext * swr;
    int16_t * conv_buf;
    int conv_buf_capacity;
};

static void pump_frame(AVFrame * fr, PumpContext * ctx)
{
    int max_out = swr_get_out_samples(ctx->swr, fr->nb_samples);
    if (ctx->conv_buf_capacity < max_out)
    {
        delete[] ctx->conv_buf;
        ctx->conv_buf_capacity = max_out;
        ctx->conv_buf = new int16_t[max_out];
    }
    uint8_t * planes[1] = {(uint8_t *)ctx->conv_buf};
    int got = swr_convert(ctx->swr, planes, max_out, (const uint8_t **)fr->data,
                          fr->nb_samples);
    if (got > 0)
    {
        if (ctx->samples_count + got > ctx->samples_capacity)
        {
            ctx->samples_capacity = (ctx->samples_count + got) * 2;
            int16_t * new_samples = new int16_t[ctx->samples_capacity];
            memcpy(new_samples, ctx->samples,
                   ctx->samples_count * sizeof(int16_t));
            delete[] ctx->samples;
            ctx->samples = new_samples;
        }
        memcpy(ctx->samples + ctx->samples_count, ctx->conv_buf,
               got * sizeof(int16_t));
        ctx->samples_count += got;
    }
}

static bool decode_peaks(const char * filename, TrackPeaks * out)
{
    AVFormatContext * fmt_ctx = nullptr;
    if (avformat_open_input(&fmt_ctx, filename, nullptr, nullptr) < 0)
        return false;
    if (avformat_find_stream_info(fmt_ctx, nullptr) < 0)
    {
        avformat_close_input(&fmt_ctx);
        return false;
    }

    int audio_idx = -1;
    for (unsigned i = 0; i < fmt_ctx->nb_streams; i++)
        if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            audio_idx = i;
            break;
        }

    if (audio_idx < 0)
    {
        avformat_close_input(&fmt_ctx);
        return false;
    }

    AVCodecParameters * params = fmt_ctx->streams[audio_idx]->codecpar;
    const AVCodec * codec = avcodec_find_decoder(params->codec_id);
    if (!codec)
    {
        avformat_close_input(&fmt_ctx);
        return false;
    }

    AVCodecContext * ctx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(ctx, params);
    if (avcodec_open2(ctx, codec, nullptr) < 0)
    {
        avcodec_free_context(&ctx);
        avformat_close_input(&fmt_ctx);
        return false;
    }

    SwrContext * swr = nullptr;

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(59, 37, 100)
    /* Modern FFmpeg (>= 5.1) Channel Layout API */
    AVChannelLayout out_layout;
    av_channel_layout_default(&out_layout, 1);

    if (swr_alloc_set_opts2(&swr, &out_layout, AV_SAMPLE_FMT_S16,
                            ctx->sample_rate, &ctx->ch_layout, ctx->sample_fmt,
                            ctx->sample_rate, 0, nullptr) < 0 ||
        !swr || swr_init(swr) < 0)
    {
        avcodec_free_context(&ctx);
        avformat_close_input(&fmt_ctx);
        return false;
    }
#else
    /* Legacy FFmpeg (< 5.1) Channel Layout API */
    uint64_t out_layout = AV_CH_LAYOUT_MONO;
    uint64_t in_layout = ctx->channel_layout;

    if (!in_layout)
        in_layout = av_get_default_channel_layout(ctx->channels);

    swr = swr_alloc_set_opts(nullptr, out_layout, AV_SAMPLE_FMT_S16,
                             ctx->sample_rate, in_layout, ctx->sample_fmt,
                             ctx->sample_rate, 0, nullptr);

    if (!swr || swr_init(swr) < 0)
    {
        if (swr)
            swr_free(&swr);
        avcodec_free_context(&ctx);
        avformat_close_input(&fmt_ctx);
        return false;
    }
#endif

    PumpContext pctx;
    pctx.samples_capacity = (size_t)ctx->sample_rate * 60 * 4;
    pctx.samples = new int16_t[pctx.samples_capacity];
    pctx.samples_count = 0;
    pctx.swr = swr;
    pctx.conv_buf_capacity = 1024;
    pctx.conv_buf = new int16_t[pctx.conv_buf_capacity];

    AVPacket * pkt = av_packet_alloc();
    AVFrame * frame = av_frame_alloc();

    while (av_read_frame(fmt_ctx, pkt) >= 0)
    {
        if (pkt->stream_index == audio_idx &&
            avcodec_send_packet(ctx, pkt) == 0)
            while (avcodec_receive_frame(ctx, frame) == 0)
                pump_frame(frame, &pctx);
        av_packet_unref(pkt);
    }
    avcodec_send_packet(ctx, nullptr);
    while (avcodec_receive_frame(ctx, frame) == 0)
        pump_frame(frame, &pctx);

    int drain;
    do
    {
        uint8_t * planes[1] = {(uint8_t *)pctx.conv_buf};
        drain = swr_convert(swr, planes, pctx.conv_buf_capacity, nullptr, 0);
        if (drain > 0)
        {
            if (pctx.samples_count + drain > pctx.samples_capacity)
            {
                pctx.samples_capacity = (pctx.samples_count + drain) * 2;
                int16_t * new_samples = new int16_t[pctx.samples_capacity];
                memcpy(new_samples, pctx.samples,
                       pctx.samples_count * sizeof(int16_t));
                delete[] pctx.samples;
                pctx.samples = new_samples;
            }
            memcpy(pctx.samples + pctx.samples_count, pctx.conv_buf,
                   drain * sizeof(int16_t));
            pctx.samples_count += drain;
        }
    } while (drain > 0);

    av_frame_free(&frame);
    av_packet_free(&pkt);
    swr_free(&swr);
    avcodec_free_context(&ctx);
    int sample_rate = fmt_ctx->streams[audio_idx]->codecpar->sample_rate;
    avformat_close_input(&fmt_ctx);

    if (pctx.samples_count == 0)
    {
        delete[] pctx.samples;
        delete[] pctx.conv_buf;
        return false;
    }

    out->resize(NUM_BUCKETS);
    out->global_peak = 1;
    out->duration_sec = (double)pctx.samples_count / sample_rate;

    for (int b = 0; b < NUM_BUCKETS; b++)
    {
        size_t start = (size_t)((double)b / NUM_BUCKETS * pctx.samples_count);
        size_t end =
            (size_t)((double)(b + 1) / NUM_BUCKETS * pctx.samples_count);
        if (end > pctx.samples_count)
            end = pctx.samples_count;
        if (end <= start)
            end = start + 1;
        if (end > pctx.samples_count)
            end = pctx.samples_count;

        int16_t mn = 0, mx = 0;
        for (size_t i = start; i < end; i++)
        {
            if (pctx.samples[i] < mn)
                mn = pctx.samples[i];
            if (pctx.samples[i] > mx)
                mx = pctx.samples[i];
        }
        out->mins[b] = mn;
        out->maxs[b] = mx;
        if (-mn > out->global_peak)
            out->global_peak = -mn;
        if (mx > out->global_peak)
            out->global_peak = mx;
    }

    delete[] pctx.samples;
    delete[] pctx.conv_buf;
    return true;
}

class WaveformWidget;
static WaveformWidget * spect_widget = nullptr;
static void load_track_async(const char * filename);

class WaveformWidget : public QWidget
{
public:
    WaveformWidget(QWidget * parent = nullptr);
    ~WaveformWidget();

    void apply_peaks(const char * filename, TrackPeaks * peaks);

protected:
    void resizeEvent(QResizeEvent *) override;
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent * event) override;
    void mouseMoveEvent(QMouseEvent * event) override;

private:
    void paint_background(QPainter &);
    void paint_waveform(QPainter &);
    void paint_playhead(QPainter &);
    void seek_to_x(int x);

    QTimer * m_timer;
};

WaveformWidget::WaveformWidget(QWidget * parent) : QWidget(parent)
{
    spect_widget = this;

    m_timer = new QTimer(this);
    QObject::connect(m_timer, &QTimer::timeout, this, [this]() { update(); });
    m_timer->start(100);

    setMinimumHeight(80);

    if (aud_drct_get_ready())
    {
        String fn = aud_drct_get_filename();
        if (fn)
            load_track_async((const char *)fn);
    }
}

WaveformWidget::~WaveformWidget() { spect_widget = nullptr; }

void WaveformWidget::apply_peaks(const char * filename, TrackPeaks * peaks)
{
    pthread_mutex_lock(&g_mutex);
    delete[] g_current_file;
    g_current_file = strdup(filename);
    delete g_peaks;
    g_peaks = peaks;
    pthread_mutex_unlock(&g_mutex);
    update();
}

void WaveformWidget::paint_background(QPainter & p)
{
    p.fillRect(0, 0, width(), height(), QColor(18, 18, 20));
}

void WaveformWidget::paint_waveform(QPainter & p)
{
    TrackPeaks local_peaks;
    pthread_mutex_lock(&g_mutex);
    if (g_peaks && g_peaks->num_buckets > 0)
    {
        local_peaks.resize(g_peaks->num_buckets);
        local_peaks.global_peak = g_peaks->global_peak;
        local_peaks.duration_sec = g_peaks->duration_sec;
        memcpy(local_peaks.mins, g_peaks->mins,
               g_peaks->num_buckets * sizeof(int16_t));
        memcpy(local_peaks.maxs, g_peaks->maxs,
               g_peaks->num_buckets * sizeof(int16_t));
    }
    pthread_mutex_unlock(&g_mutex);

    int w = width(), h = height();
    int center_y = h / 2;

    p.setPen(QColor(90, 90, 95));
    p.drawLine(0, center_y, w, center_y);

    if (local_peaks.num_buckets == 0)
        return;

    double half = h / 2.0;
    double scale = (half * 0.95) / (double)local_peaks.global_peak;
    int n = local_peaks.num_buckets;

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(64, 200, 255));

    for (int x = 0; x < w; x++)
    {
        int idx = x * n / w;
        if (idx >= n)
            idx = n - 1;
        double top = center_y - local_peaks.maxs[idx] * scale;
        double bot = center_y - local_peaks.mins[idx] * scale;
        if (bot < top)
        {
            double tmp = top;
            top = bot;
            bot = tmp;
        }
        p.drawRect(QRectF(x, top, 1.0, bot - top + 1.0));
    }
}

void WaveformWidget::paint_playhead(QPainter & p)
{
    if (!aud_drct_get_playing())
        return;

    int length = aud_drct_get_length();
    if (length <= 0)
        return;

    int time = aud_drct_get_time();
    double frac = (double)time / length;
    if (frac < 0)
        frac = 0;
    if (frac > 1)
        frac = 1;

    double x = frac * width();

    QPen pen(QColor(255, 40, 40));
    pen.setWidth(2);
    p.setPen(pen);
    p.drawLine(QPointF(x, 0), QPointF(x, height()));
}

void WaveformWidget::resizeEvent(QResizeEvent *) { update(); }

void WaveformWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    paint_background(p);
    paint_waveform(p);
    paint_playhead(p);
}

void WaveformWidget::seek_to_x(int x)
{
    int length = aud_drct_get_length();
    if (length <= 0 || width() <= 0)
        return;

    double frac = (double)x / width();
    if (frac < 0)
        frac = 0;
    if (frac > 1)
        frac = 1;

    aud_drct_seek((int)(frac * length));
    update();
}

void WaveformWidget::mousePressEvent(QMouseEvent * event)
{
    if (event->button() == Qt::LeftButton)
        seek_to_x(event->pos().x());
}

void WaveformWidget::mouseMoveEvent(QMouseEvent * event)
{
    if (event->buttons() & Qt::LeftButton)
        seek_to_x(event->pos().x());
}

// async loader thread helper
static void * worker_thread_func(void * arg)
{
    char * filename = (char *)arg;
    TrackPeaks * peaks = new TrackPeaks();
    char * cpath = cache_path_for(filename);

    bool ok = load_cache(cpath, peaks);
    if (!ok)
    {
        StringBuf local = uri_to_filename(filename);
        const char * decode_path = local ? (const char *)local : filename;

        ok = decode_peaks(decode_path, peaks);
        if (ok)
            save_cache(cpath, peaks);
    }
    delete[] cpath;

    if (!ok)
    {
        delete peaks;
        free(filename);
        return nullptr;
    }

    /* dynamic copies passed along safely to the Qt event loop lambda */
    char * fn_copy = strdup(filename);
    QMetaObject::invokeMethod(
        qApp,
        [fn_copy, peaks]() {
            if (spect_widget)
                spect_widget->apply_peaks(fn_copy, peaks);
            else
            {
                pthread_mutex_lock(&g_mutex);
                delete[] g_current_file;
                g_current_file = strdup(fn_copy);
                delete g_peaks;
                g_peaks = peaks;
                pthread_mutex_unlock(&g_mutex);
            }
            free(fn_copy);
        },
        Qt::QueuedConnection);

    free(filename);
    return nullptr;
}

static void load_track_async(const char * filename)
{
    pthread_mutex_lock(&g_mutex);
    if (g_current_file && strcmp(filename, g_current_file) == 0)
    {
        pthread_mutex_unlock(&g_mutex);
        return;
    }
    pthread_mutex_unlock(&g_mutex);

    char * fn_arg = strdup(filename);
    pthread_t thread;
    if (pthread_create(&thread, nullptr, worker_thread_func, fn_arg) == 0)
    {
        pthread_detach(thread);
    }
    else
    {
        free(fn_arg);
    }
}

static void on_playback_ready(void *, void *)
{
    String fn = aud_drct_get_filename();
    if (fn)
        load_track_async((const char *)fn);
}

class QtWaveform : public VisPlugin
{
public:
    static constexpr PluginInfo info = {N_("Waveform"), PACKAGE, nullptr,
                                        nullptr, PluginQtOnly};

    constexpr QtWaveform() : VisPlugin(info, Visualizer::Freq) {}

    bool init() override;
    void cleanup() override;
    void * get_qt_widget() override;
    void clear() override;
    void render_freq(const float * freq) override;
};

EXPORT QtWaveform aud_plugin_instance;

bool QtWaveform::init()
{
    hook_associate("playback ready", (HookFunction)on_playback_ready, nullptr);
    return true;
}

void QtWaveform::cleanup()
{
    hook_dissociate("playback ready", (HookFunction)on_playback_ready);
    pthread_mutex_lock(&g_mutex);
    delete[] g_current_file;
    g_current_file = nullptr;
    delete g_peaks;
    g_peaks = nullptr;
    pthread_mutex_unlock(&g_mutex);
}

void QtWaveform::render_freq(const float * freq) {}

void QtWaveform::clear()
{
    if (spect_widget)
        spect_widget->update();
}

void * QtWaveform::get_qt_widget()
{
    if (spect_widget)
        return spect_widget;

    spect_widget = new WaveformWidget();
    return spect_widget;
}