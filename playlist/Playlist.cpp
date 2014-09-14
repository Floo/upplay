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
using namespace std;

#include <QList>
#include <QDebug>


#include "Playlist.h"

Playlist::Playlist(QObject * parent) 
    : QObject (parent)
{
    _settings = CSettingsStorage::getInstance();
    _playlist_mode = _settings->getPlaylistMode();
    m_meta.clear();
    m_play_idx = -1;
    _pause = false;
}

// Remove one row
void Playlist::remove_row(int row)
{
    qDebug() << "Playlist::remove_row";
    QList<int> remove_list;
    remove_list << row;
    psl_remove_rows(remove_list);
}

void Playlist::psl_new_transport_state(int tps, const char *s)
{
    qDebug() << "Playlist::psl_new_transport_state " << s <<
        " play_idx " << m_play_idx;
    if (m_play_idx >= 0 && m_play_idx < m_meta.size()) 
        qDebug() << "     meta[idx].pl_playing " << m_meta[m_play_idx].pl_playing;
    switch (tps) {
    case AUDIO_UNKNOWN:
    case AUDIO_STOPPED:
    default:
        emit sig_stopped();
        break;
    case AUDIO_PLAYING:
        emit sig_playing();
        break;
    case AUDIO_PAUSED:
        emit sig_paused();
        break;
    }
}

// GUI -->
void Playlist::psl_change_mode(const Playlist_Mode& mode)
{
    _settings->setPlaylistMode(mode);
    _playlist_mode = mode;
    mode.print();
    emit sig_mode_changed(mode);
}

// Audio -->
void Playlist::psl_mode_changed(Playlist_Mode mode)
{
    _playlist_mode = mode;
}

void Playlist::psl_append_tracks(const MetaDataList& v_md)
{
    qDebug() << "Playlist::psl_append_tracks()";
    psl_insert_tracks(v_md, m_meta.size() - 1);
}
