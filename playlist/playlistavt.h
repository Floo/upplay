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
 */
#ifndef PLAYLISTAVT_H_
#define PLAYLISTAVT_H_

#include <iostream>

#include <QObject>
#include <QList>
#include <QMap>
#include <QStringList>

#include "HelperStructs/MetaData.h"
#include "HelperStructs/PlaylistMode.h"
#include "HelperStructs/globals.h"
#include "HelperStructs/CSettingsStorage.h"
#include "playlist.h"
#include "upadapt/avtadapt.h"

class PlaylistAVT : public Playlist {
    Q_OBJECT

public:
    PlaylistAVT(QObject *parent = 0);
    PlaylistAVT(AVTPlayer *avtp, const std::string& UDN, QObject *parent = 0);
    virtual ~PlaylistAVT() {}

public slots:
    // Insert after idx. Use -1 to insert at start
    void psl_insert_tracks(const MetaDataList&, int idx);

    void psl_change_track_impl(int);
    void psl_prepare_for_end_of_track();
    void psl_ext_track_change(const QString& uri);
    void psl_onCurrentMetadata(const MetaData&);
    void psl_clear_playlist_impl();
    void psl_play();
    void psl_pause();
    void psl_stop();
    void psl_forward();
    void psl_backward();
    void psl_remove_rows(const QList<int> &, bool select_next=true);
    void psl_seek(int);
    
signals:
    // This is for player action
    void sig_play_now(const MetaData&, int pos = 0, bool play = true);

    // We send this when approaching the end of the current track, or
    // if the following track changes. This allows the audio to
    // prepare for gaplessness
    void sig_next_track_to_play(const MetaData&);

protected:
    void set_for_playing(int row);
    void send_next_playing_signal();
    bool checkTrack(const MetaData&) {return true;}

private slots:
    void playlist_updated();
private:
    AVTPlayer *m_avto;
    std::string m_savefile;
};

#endif /* PLAYLISTAVT_H_ */
