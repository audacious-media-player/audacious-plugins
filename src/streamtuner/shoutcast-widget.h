// Copyright (c) 2019 Ariadne Conill <ariadne@dereferenced.org>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// This software is provided 'as is' and without any warranty, express or
// implied.  In no event shall the authors be liable for any damages arising
// from the use of this software.

#ifndef STREAMTUNER_SHOUTCAST_WIDGET_H
#define STREAMTUNER_SHOUTCAST_WIDGET_H

#include <libaudqt/treeview.h>

#include <QWidget>
#include <QTabWidget>
#include <QAbstractListModel>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

#include "shoutcast-model.h"

class ShoutcastTunerWidget : public audqt::TreeView {
public:
     ShoutcastTunerWidget(QWidget * parent = nullptr);

     void activate (const QModelIndex & index);

private:
     ShoutcastTunerModel *m_model;
};

#endif
