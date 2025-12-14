/*****************************************************************************/
/*                                                                           */
/*  Copyright (C) 2025 Yves Ndiaye                                           */
/*                                                                           */
/* This Source Code Form is subject to the terms of the Mozilla Public       */
/* License, v. 2.0. If a copy of the MPL was not distributed with this       */
/* file, You can obtain one at https://mozilla.org/MPL/2.0/.                 */
/*                                                                           */
/*****************************************************************************/

#import <Cocoa/Cocoa.h>
#import <Foundation/Foundation.h>
#import <MediaPlayer/MediaPlayer.h>
#include <fstream>
#include <iostream>
#include <istream>

#include <libaudcore/drct.h>
#include <libaudcore/hook.h>
#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>
#include <libaudcore/probe.h>

class MacNowPlayingPlugin : public GeneralPlugin
{
public:
    static const char about[];
    static constexpr PluginInfo info = {N_("Mac Now Playing"), PACKAGE, about};

    constexpr MacNowPlayingPlugin() : GeneralPlugin(info, true) {}

    bool init();
    void cleanup();
};

EXPORT MacNowPlayingPlugin aud_plugin_instance;

static void update_metadata(void * data, void * user);

static void update_player_time()
{
    if (!aud_drct_get_ready())
    {
        return;
    }
    MPNowPlayingInfoCenter * center = [MPNowPlayingInfoCenter defaultCenter];
    double rate =
        [center playbackState] == MPNowPlayingPlaybackStatePlaying ? 1. : 0.;
    int current_time = aud_drct_get_time() / 1000;
    NSDictionary<NSString *, id> * info = [center nowPlayingInfo];
    NSMutableDictionary<NSString *, id> * minfo = [info mutableCopy];
    minfo[MPNowPlayingInfoPropertyPlaybackRate] =
        [[NSNumber alloc] initWithDouble:rate];
    minfo[MPNowPlayingInfoPropertyElapsedPlaybackTime] =
        [[NSNumber alloc] initWithInt:current_time];
    center.nowPlayingInfo = minfo;
}

static void remote_command_setup_handler()
{
    // No need to change the now playing status since it will be cought by the
    // hook.
    MPRemoteCommandCenter * commandCenter =
        [MPRemoteCommandCenter sharedCommandCenter];
    // Play
    [[commandCenter playCommand] setEnabled:YES];
    [[commandCenter playCommand]
        addTargetWithHandler:^MPRemoteCommandHandlerStatus(
            MPRemoteCommandEvent * _Nonnull event) {
          aud_drct_play();
          return MPRemoteCommandHandlerStatusSuccess;
        }];
    // Pause
    [[commandCenter pauseCommand] setEnabled:YES];
    [[commandCenter pauseCommand]
        addTargetWithHandler:^MPRemoteCommandHandlerStatus(
            MPRemoteCommandEvent * _Nonnull event) {
          aud_drct_pause();
          return MPRemoteCommandHandlerStatusSuccess;
        }];
    // Toggle playpause
    [[commandCenter togglePlayPauseCommand] setEnabled:YES];
    [[commandCenter togglePlayPauseCommand]
        addTargetWithHandler:^MPRemoteCommandHandlerStatus(
            MPRemoteCommandEvent * _Nonnull event) {
          aud_drct_play_pause();
          return MPRemoteCommandHandlerStatusSuccess;
        }];
    // Skip forward
    [[commandCenter nextTrackCommand] setEnabled:YES];
    [[commandCenter nextTrackCommand]
        addTargetWithHandler:^MPRemoteCommandHandlerStatus(
            MPRemoteCommandEvent * _Nonnull event) {
          aud_drct_pl_next();
          return MPRemoteCommandHandlerStatusSuccess;
        }];
    // Skip backward
    [[commandCenter previousTrackCommand] setEnabled:YES];
    [[commandCenter previousTrackCommand]
        addTargetWithHandler:^MPRemoteCommandHandlerStatus(
            MPRemoteCommandEvent * _Nonnull event) {
          aud_drct_pl_prev();
          return MPRemoteCommandHandlerStatusSuccess;
        }];
    // Seek
    [[commandCenter changePlaybackPositionCommand] setEnabled:YES];
    [[commandCenter changePlaybackPositionCommand]
        addTargetWithHandler:^MPRemoteCommandHandlerStatus(
            MPRemoteCommandEvent * _Nonnull event) {
          if (aud_drct_get_playing())
          {
              MPChangePlaybackPositionCommandEvent * position_event =
                  (MPChangePlaybackPositionCommandEvent *)event;
              // Apple time is in second
              aud_drct_seek((int)position_event.positionTime * 1000);
          }
          return MPRemoteCommandHandlerStatusSuccess;
        }];
}

static void remote_command_disable_commands()
{
    MPRemoteCommandCenter * commands =
        [MPRemoteCommandCenter sharedCommandCenter];
    commands.seekBackwardCommand.enabled = NO;
    commands.seekForwardCommand.enabled = NO;
    commands.changeRepeatModeCommand.enabled = NO;
    commands.changeShuffleModeCommand.enabled = NO;
    commands.skipBackwardCommand.enabled = NO;
    commands.enableLanguageOptionCommand.enabled = NO;
    commands.disableLanguageOptionCommand.enabled = NO;
    commands.ratingCommand.enabled = NO;
    commands.likeCommand.enabled = NO;
    commands.dislikeCommand.enabled = NO;
    commands.bookmarkCommand.enabled = NO;
}

static void remote_command_set_playback_state()
{
    MPNowPlayingInfoCenter * center = [MPNowPlayingInfoCenter defaultCenter];
    MPNowPlayingPlaybackState state = MPNowPlayingPlaybackStateStopped;
    if (aud_drct_get_playing())
    {
        bool paused = aud_drct_get_paused();
        state = paused ? MPNowPlayingPlaybackStatePaused
                       : MPNowPlayingPlaybackStatePlaying;
    }
    [center setPlaybackState:state];
}

static void remote_command_start()
{
    MPNowPlayingInfoCenter * center = [MPNowPlayingInfoCenter defaultCenter];
    // Little hack to get the now playing starting otherwise Audacious
    // doesn't react to media keys
    [center setPlaybackState:MPNowPlayingPlaybackStatePaused];
    [center setPlaybackState:MPNowPlayingPlaybackStatePlaying];
    remote_command_set_playback_state();
    update_metadata(nullptr, nullptr);
}

static void update_position(void * data, void * user) { update_player_time(); }

static void update_metadata(void * data, void * user)
{
    MPNowPlayingInfoCenter * center = [MPNowPlayingInfoCenter defaultCenter];
    if (!aud_drct_get_ready())
    {
        return;
    }
    Tuple tuple = aud_drct_get_tuple();
    String title = tuple.get_str(Tuple::Title);
    String artist = tuple.get_str(Tuple::Artist);
    String album = tuple.get_str(Tuple::Album);
    String file = aud_drct_get_filename();
    int current_time = aud_drct_get_time();
    int length = tuple.get_int(Tuple::Length);

    NSMutableDictionary<NSString *, id> * nowPlayingInfo =
        [[NSMutableDictionary alloc] init];
    nowPlayingInfo[MPNowPlayingInfoPropertyMediaType] =
        [[NSNumber alloc] initWithInt:MPNowPlayingInfoMediaTypeAudio];
    nowPlayingInfo[MPNowPlayingInfoPropertyPlaybackRate] =
        [[NSNumber alloc] initWithBool:aud_drct_get_playing()];
    nowPlayingInfo[MPMediaItemPropertyTitle] =
        [[NSString alloc] initWithUTF8String:title ? title : ""];
    nowPlayingInfo[MPMediaItemPropertyArtist] =
        [[NSString alloc] initWithUTF8String:artist ? artist : ""];
    nowPlayingInfo[MPMediaItemPropertyAlbumTitle] =
        [[NSString alloc] initWithUTF8String:album ? album : ""];
    nowPlayingInfo[MPMediaItemPropertyMediaType] =
        [[NSNumber alloc] initWithInt:MPMediaTypeMusic];
    nowPlayingInfo[MPMediaItemPropertyPlaybackDuration] =
        [[NSNumber alloc] initWithInt:length / 1000];
    nowPlayingInfo[MPNowPlayingInfoPropertyElapsedPlaybackTime] =
        [[NSNumber alloc] initWithInt:current_time];

    if (file)
    {
        AudArtPtr artwork = aud_art_request(file, AUD_ART_DATA);
        const Index<char> * artworkBytes = artwork.data();
        if (artworkBytes)
        {
            NSData * nsBytes =
                [[NSData alloc] initWithBytes:artworkBytes->begin()
                                       length:artworkBytes->len()];
            NSImage * image = [[NSImage alloc] initWithData:nsBytes];
            MPMediaItemArtwork * mpArtwork = [[MPMediaItemArtwork alloc]
                initWithBoundsSize:[image size]
                    requestHandler:^NSImage * _Nonnull(CGSize size) {
                      return image;
                    }];
            nowPlayingInfo[MPMediaItemPropertyArtwork] = mpArtwork;
        }
    }
    [center setNowPlayingInfo:nowPlayingInfo];
}

static void update_playback_status(void * data, void * user)
{
    remote_command_set_playback_state();
    update_player_time();
}

const char MacNowPlayingPlugin::about[] =
    N_("Now Playing Plugin for Audacious\n"
       "Copyright 2025 Yves Ndiaye\n\n"
       "This plugin implements the Apple Now Playing interface. "
       "It allows Audacious to be controlled through media keys, "
       "to show the current album artwork and to seek the playback.");

bool MacNowPlayingPlugin::init()
{
    remote_command_setup_handler();
    remote_command_start();
    remote_command_disable_commands();

    hook_associate("playback begin", update_playback_status, nullptr);
    hook_associate("playback pause", update_playback_status, nullptr);
    hook_associate("playback stop", update_playback_status, nullptr);
    hook_associate("playback unpause", update_playback_status, nullptr);
    hook_associate("playback ready", update_metadata, nullptr);
    hook_associate("playback stop", update_metadata, nullptr);
    hook_associate("tuple change", update_metadata, nullptr);
    hook_associate("playback ready", update_position, nullptr);
    hook_associate("playback seek", update_position, nullptr);
    return true;
}

void MacNowPlayingPlugin::cleanup()
{
    hook_dissociate("playback begin", update_playback_status);
    hook_dissociate("playback pause", update_playback_status);
    hook_dissociate("playback stop", update_playback_status);
    hook_dissociate("playback unpause", update_playback_status);
    hook_dissociate("playback ready", update_metadata);
    hook_dissociate("playback stop", update_metadata);
    hook_dissociate("tuple change", update_metadata);
    hook_dissociate("playback ready", update_position);
    hook_dissociate("playback seek", update_position);
}
