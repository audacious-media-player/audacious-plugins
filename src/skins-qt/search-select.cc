/*  Audacious - Cross-platform multimedia player
 *  Copyright (C) 2005-2011  Audacious development team.
 *
 *  BMP - Cross-platform multimedia player
 *  Copyright (C) 2003-2004  BMP development team.
 *
 *  Based on XMMS:
 *  Copyright (C) 1998-2003  XMMS development team.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; under version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses>.
 *
 *  The Audacious team does not consider modular code linking to
 *  Audacious or using our public API to be a derived work.
 */

#include "actions-playlist.h"
#include "playlistwin.h"
#include "playlist-widget.h"

#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

#include <libaudcore/i18n.h>
#include <libaudcore/playlist.h>
#include <libaudqt/libaudqt.h>

class SearchSelectDialog : public QDialog
{
public:
    SearchSelectDialog (QWidget * parent = nullptr);

private:
    void toggle_related_checkbox (QCheckBox * caller, QCheckBox * related);
    void copy_selected_to_new (Playlist playlist);
    void focus_first_selected_entry (Playlist playlist);
    void search ();

    QLineEdit * m_entry_title, * m_entry_album,
              * m_entry_artist, * m_entry_file_name;
    QCheckBox * m_checkbox_clearprevsel,
              * m_checkbox_autoenqueue,
              * m_checkbox_newplaylist;
};

SearchSelectDialog::SearchSelectDialog (QWidget * parent) : QDialog (parent)
{
    setWindowTitle (_("Search entries in active playlist"));
    setWindowRole ("search");
    setContentsMargins (audqt::margins.FourPt);

    auto logo = new QLabel;
    int logo_size = audqt::to_native_dpi (48);
    logo->setPixmap (QIcon::fromTheme ("edit-find").pixmap (logo_size, logo_size));

    auto help_text = new QLabel (_("Select entries in playlist by filling one or more "
     "fields. Fields use regular expressions syntax, case-insensitive. If you don't know how "
     "regular expressions work, simply insert a literal portion of what you're searching for."));
    help_text->setWordWrap (true);

    auto label_title = new QLabel (_("Title:"));
    m_entry_title = new QLineEdit;
    m_entry_title->setFocus ();

    auto label_album = new QLabel (_("Album:"));
    m_entry_album = new QLineEdit;

    auto label_artist = new QLabel (_("Artist:"));
    m_entry_artist = new QLineEdit;

    auto label_file_name = new QLabel (_("File Name:"));
    m_entry_file_name = new QLineEdit;

    m_checkbox_clearprevsel = new QCheckBox (_("Clear previous selection before searching"));
    m_checkbox_autoenqueue = new QCheckBox (_("Automatically toggle queue for matching entries"));
    m_checkbox_newplaylist = new QCheckBox (_("Create a new playlist with matching entries"));
    m_checkbox_clearprevsel->setCheckState (Qt::Checked);

    QObject::connect (m_checkbox_autoenqueue, & QAbstractButton::toggled, [this] () {
        toggle_related_checkbox (m_checkbox_autoenqueue, m_checkbox_newplaylist);
    });

    QObject::connect (m_checkbox_newplaylist, & QAbstractButton::toggled, [this] () {
        toggle_related_checkbox (m_checkbox_newplaylist, m_checkbox_autoenqueue);
    });

    auto search = new QPushButton (_("Search"));
    auto cancel = new QPushButton (_("Cancel"));

    auto buttonbox = new QDialogButtonBox;
    buttonbox->addButton (search, QDialogButtonBox::AcceptRole);
    buttonbox->addButton (cancel, QDialogButtonBox::RejectRole);

    QObject::connect (buttonbox, & QDialogButtonBox::accepted, this, & SearchSelectDialog::search);
    QObject::connect (buttonbox, & QDialogButtonBox::rejected, this, & QDialog::close);

    auto hbox = audqt::make_hbox (nullptr);
    hbox->addWidget (logo);
    hbox->addWidget (help_text);

    auto grid = new QGridLayout;
    grid->addLayout (hbox, 0, 0, 1, 2, Qt::AlignLeft);
    grid->addWidget (label_title, 1, 0, Qt::AlignRight);
    grid->addWidget (m_entry_title, 1, 1);
    grid->addWidget (label_album, 2, 0, Qt::AlignRight);
    grid->addWidget (m_entry_album, 2, 1);
    grid->addWidget (label_artist, 3, 0, Qt::AlignRight);
    grid->addWidget (m_entry_artist, 3, 1);
    grid->addWidget (label_file_name, 4, 0, Qt::AlignRight);
    grid->addWidget (m_entry_file_name, 4, 1);
    grid->addWidget (m_checkbox_clearprevsel, 5, 0, 1, 2);
    grid->addWidget (m_checkbox_autoenqueue, 6, 0, 1, 2);
    grid->addWidget (m_checkbox_newplaylist, 7, 0, 1, 2);

    auto vbox = audqt::make_vbox (this);
    vbox->addLayout (grid);
    vbox->addWidget (buttonbox, 0, Qt::AlignBottom);
}

void SearchSelectDialog::toggle_related_checkbox (QCheckBox * caller, QCheckBox * related)
{
    if (caller->isChecked ())
        related->setCheckState (Qt::Unchecked);
}

void SearchSelectDialog::copy_selected_to_new (Playlist playlist)
{
    int entries = playlist.n_entries ();
    Index<PlaylistAddItem> items;

    for (int entry = 0; entry < entries; entry ++)
    {
        if (playlist.entry_selected (entry))
        {
            items.append
             (playlist.entry_filename (entry),
              playlist.entry_tuple (entry, Playlist::NoWait),
              playlist.entry_decoder (entry, Playlist::NoWait));
        }
    }

    auto new_list = Playlist::new_playlist ();
    new_list.insert_items (0, std::move (items), false);
}

void SearchSelectDialog::focus_first_selected_entry (Playlist playlist)
{
    int entries = playlist.n_entries ();
    for (int i = 0; i < entries; i ++)
    {
        if (playlist.entry_selected (i))
        {
            playlistwin_list->set_focused (i);
            return;
        }
    }
}

void SearchSelectDialog::search ()
{
    auto playlist = Playlist::active_playlist ();

    /* create a tuple with user search data */
    Tuple tuple;
    tuple.set_str (Tuple::Title, m_entry_title->text ().toUtf8 ());
    tuple.set_str (Tuple::Album, m_entry_album->text ().toUtf8 ());
    tuple.set_str (Tuple::Artist, m_entry_artist->text ().toUtf8 ());
    tuple.set_str (Tuple::Basename, m_entry_file_name->text ().toUtf8 ());

    /* check if previous selection should be cleared before searching */
    if (m_checkbox_clearprevsel->isChecked ())
        playlist.select_all (false);

    playlist.select_by_patterns (tuple);

    /* check if a new playlist should be created after searching */
    if (m_checkbox_newplaylist->isChecked ())
        copy_selected_to_new (playlist);
    else
    {
        /* set focus on the first entry found */
        focus_first_selected_entry (playlist);

        /* check if matched entries should be queued */
        if (m_checkbox_autoenqueue->isChecked ())
            playlist.queue_insert_selected (-1);
    }

    /* done here :) */
    close ();
}

void action_playlist_search_and_select ()
{
    auto dialog = new SearchSelectDialog;
    dialog->setAttribute (Qt::WA_DeleteOnClose);
    dialog->show ();
}
