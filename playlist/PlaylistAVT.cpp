/* Copyright (C) 2011  Lucio Carreras
 *
 * This file is part of sayonara player
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <ctime>

#include <QFile>
#include <QList>
#include <QObject>
#include <QDate>
#include <QTime>
#include <QDebug>
#include <QDir>
#include <QTimer>

#include <libupnpp/control/avtransport.hxx>

#include "PlaylistAVT.h"
#include "HelperStructs/Helper.h"
#include "upadapt/upputils.h"

using namespace std;

PlaylistAVT::PlaylistAVT(QObject *parent)
  : Playlist(parent)
{
    QTimer::singleShot(0, this, SLOT(playlist_updated()));
}

PlaylistAVT::PlaylistAVT(const string& _udn, QObject *parent)
  : Playlist(parent)
{
    string udn(_udn);
    if (udn.find("uuid:") == 0) {
        udn = udn.substr(5);
    }

    m_savefile = qs2utf8s(Helper::getHomeDataPath()) + string("sq-") +
        (udn.empty() ? "savedQueue" : udn);

    // We used to call these "savedQueue". Rename on first use. This
    // can go away in a few releases.
    string oldsaved = qs2utf8s(Helper::getHomeDataPath()) + "savedQueue";
    if (!udn.empty()) {
        if (access(oldsaved.c_str(), R_OK) == 0 && 
            access(m_savefile.c_str(), 0) != 0) {
            rename(oldsaved.c_str(), m_savefile.c_str());
        }
    }

    m_meta.unSerialize(u8s2qs(m_savefile));
    QTimer::singleShot(0, this, SLOT(playlist_updated()));
}

void PlaylistAVT::set_for_playing(int row)
{
    qDebug() << "PlaylistAVT::set_for_playing " << row;
    if (row < 0 || row >= int(m_meta.size())) {
        m_play_idx = -1;
        m_meta.setCurPlayTrack(-1);
        return;
    }

    m_play_idx = row;
    m_meta.setCurPlayTrack(row);
    
    emit sig_playing_track_changed(row);
    emit sig_play_now(m_meta[row]);
    emit sig_track_metadata(m_meta[row]);
}

// Player switched tracks under us. Hopefully the uri matches another
// track.  We first look ahead, because the normal situation is that
// the device switched to the nextURI track
void PlaylistAVT::psl_ext_track_change(const QString& uri)
{
    qDebug() << "PlaylistAVT::psl_ext_track_change " << uri;

    if (m_play_idx < -1) // ??
        return;

    for (unsigned int i = m_play_idx + 1; i < m_meta.size(); i++) {
        if (!uri.compare(m_meta[i].filepath)) {
            qDebug() << "PlaylistAVT::psl_ext_track_change: index now " << i;
            m_play_idx = i;
            m_meta.setCurPlayTrack(i);
            emit sig_playing_track_changed(i);
            emit sig_track_metadata(m_meta[i]);
            return;
        }
    }
    for (int i = 0; i <= m_play_idx && i < int(m_meta.size()); i++) {
        if (!uri.compare(m_meta[i].filepath)) {
            qDebug() << "PlaylistAVT::psl_ext_track_change: index now " << i;
            m_play_idx = i;
            m_meta.setCurPlayTrack(i);
            emit sig_playing_track_changed(i);
            emit sig_track_metadata(m_meta[i]);
            return;
        }
    }
}

void PlaylistAVT::psl_onCurrentMetadata(const MetaData& md)
{
    if (!m_meta.contains(md, true)) {
        m_meta.push_back(md);
        playlist_updated();
    }
    emit sig_track_metadata(md);
}

void PlaylistAVT::send_next_playing_signal()
{
    // Do not do this in shuffle mode: makes no sense + as we never go
    // through the stop state, shuffle does not work
    if (CSettingsStorage::getInstance()->getPlaylistMode().shuffle) 
        return;
    // Only if there is a track behind the current one
    if (m_play_idx >= 0 && m_play_idx < int(m_meta.size()) - 1)
        emit sig_next_track_to_play(m_meta[m_play_idx + 1]);
}

void PlaylistAVT::psl_prepare_for_end_of_track()
{
    if (!valid_row(m_play_idx))
        return;
    send_next_playing_signal();
}

void PlaylistAVT::psl_next_track()
{
    qDebug() << "PlaylistAVT::psl_next_track()";

    int track_num = -1;
    if(m_meta.empty()) {
        qDebug() << "PlaylistAVT::psl_next_track(): empty playlist";
        goto out;
    }

    if (CSettingsStorage::getInstance()->getPlaylistMode().shuffle) {
        // shuffle mode
        track_num = rand() % m_meta.size();
        if (track_num == m_play_idx) {
            track_num = (m_play_idx + 1) % m_meta.size();
        }
    } else {
        if (m_play_idx >= int(m_meta.size()) -1) {
            // last track
            qDebug() << "PlaylistAVT::psl_next_track(): was last, stop or loop";
            if (CSettingsStorage::getInstance()->getPlaylistMode().repAll) {
                track_num = 0;
            }
        } else {
            track_num = m_play_idx + 1;
            qDebug() << "PlaylistAVT::psl_next_track(): new tnum " << track_num;
        }
    }

out:
    if (track_num >= 0) {
        // valid next track
        if(checkTrack(m_meta[track_num])){
            set_for_playing(track_num);
        } else {
            remove_row(track_num);
            psl_next_track();
        }
    } else {
        set_for_playing(-1);
        emit sig_stop();
        return;
    }
}

void PlaylistAVT::psl_clear_playlist_impl()
{
    emit sig_stop();
    playlist_updated();
}

void PlaylistAVT::psl_play()
{
    m_pause = false;

    if (m_meta.empty()) {
        return;
    }

    if (m_play_idx < 0) {
        if (valid_row(m_selection_min_row)) {
            set_for_playing(m_selection_min_row);
        } else {
            set_for_playing(0);
        }
    } else {
        emit sig_resume_play();
    }
}

void PlaylistAVT::psl_pause()
{
    m_pause = true;
    emit sig_pause();
}

void PlaylistAVT::psl_stop()
{
    set_for_playing(-1);
    emit sig_stop();
    playlist_updated();
}

// fwd was pressed -> next track
void PlaylistAVT::psl_forward()
{
    psl_next_track();
}

// GUI -->
void PlaylistAVT::psl_backward()
{
    if (m_play_idx <= 0) {
        return;
    }

    int track_num = m_play_idx - 1;

    if (checkTrack(m_meta[track_num])) {
        set_for_playing(track_num);
    }
}

// GUI -->
void PlaylistAVT::psl_change_track_impl(int new_row)
{
    if (!valid_row(new_row)) {
        return;
    }

    if (checkTrack(m_meta[new_row])) {
        set_for_playing(new_row);
    } else {
        set_for_playing(-1);
        remove_row(new_row);
        emit sig_stop();
        playlist_updated();
    }
}

// insert tracks after idx which may be -1
void PlaylistAVT::psl_insert_tracks(const MetaDataList& nmeta, int row)
{
    // Change to more conventional "insert before"
    ++row;

    qDebug() << "PlaylistAVT::psl_insert_tracks: cur size" << 
        m_meta.size() << ". " << nmeta.size() << " rows before row " << row;
    if (m_meta.empty()) {
        if (row != 0) {
            return;
        }
    } else if (row < 0 || row > int(m_meta.size())) {
        qDebug() << " bad row";
        return;
    }
    
    m_meta.insert(m_meta.begin() + row, nmeta.begin(), nmeta.end());

    // Find the playing track (could be part of nmeta if this is a d&d
    // cut/paste) and adjust m_play_idx
    m_play_idx = -1;
    for (auto it = m_meta.begin(); it != m_meta.end(); it++) {
        if (it->pl_playing && m_play_idx == -1) {
            m_play_idx = it - m_meta.begin();
        } else {
            it->pl_playing = false;
        }
    }

    // Prepare following track
    send_next_playing_signal();

    playlist_updated();
}

void PlaylistAVT::playlist_updated()
{
    emit sig_playlist_updated(m_meta, m_play_idx, 0);
    string tmp(m_savefile + "-");
    m_meta.serialize(u8s2qs(tmp));
    rename(tmp.c_str(),  m_savefile.c_str());
}

void PlaylistAVT::psl_remove_rows(const QList<int>& rows, bool select_next_row)
{
    qDebug() << "PlaylistAVT::psl_remove_rows";
    if (rows.empty()) {
        return;
    }

    if (rows.contains(m_play_idx)) {
        m_play_idx = -1;
        emit sig_stop();
    }

    MetaDataList v_tmp_md;
    int n_tracks_before_cur_idx = 0;
    int first_row = rows[0];
    for (unsigned int i = 0; i < m_meta.size(); i++) {
        if (rows.contains(i)) {
            if (int(i) < m_play_idx) {
                n_tracks_before_cur_idx++;
            }
        } else {
            MetaData md = m_meta[i];
            md.pl_dragged = false;
            md.pl_selected = false;
            md.pl_playing = false;
            v_tmp_md.push_back(md);
        }
    }

    m_play_idx -= n_tracks_before_cur_idx;
    m_meta = v_tmp_md;

    if (m_play_idx < 0 || m_play_idx >= (int) m_meta.size()) {
        m_play_idx = -1;
    } else {
        m_meta[m_play_idx].pl_playing = true;
    }

    if (select_next_row) {
        m_meta[first_row].pl_selected = true;
    }

    playlist_updated();
    emit sig_playlist_updated(m_meta, m_play_idx, 0);
}
