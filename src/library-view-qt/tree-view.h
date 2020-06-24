#pragma once

#include <libaudqt/treeview.h>

#include "directory-structure-model.h"
#include "library-view-qt.h"

class LibraryTreeView : public audqt::TreeView {
  public:
    LibraryTreeView(QWidget* parent = 0);
    void set_root_path(String root_path = get_pref_uri());

  private:
    void selectionChanged(const QItemSelection& selected,
                          const QItemSelection& deselected);
    DirectoryStructureModel* directory_structure_model = nullptr;

  protected:
    bool eventFilter(QObject* object, QEvent* event);
};
