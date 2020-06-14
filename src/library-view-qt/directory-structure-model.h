#pragma once
#include <QFileSystemModel>

class DirectoryStructureModel : public QFileSystemModel
{
	public:
	DirectoryStructureModel(QObject* parent = 0);
	QModelIndex set_root_path(QString path);
	int		columnCount(const QModelIndex& index = QModelIndex()) const override;
};