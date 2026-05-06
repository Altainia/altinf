#pragma once

#include "auth/session_data.hpp"
#include "kanban/kanban_db.hpp"
#include "org/org_db.hpp"

#include <Wt/WColorPicker.h>
#include <Wt/WContainerWidget.h>
#include <Wt/WLineEdit.h>
#include <Wt/WText.h>

#include <memory>

class org_type_manager_page: public Wt::WContainerWidget
{
public:
    org_type_manager_page(kanban_db&          db,
                           org_db&             odb,
                           long long           org_id,
                           const session_data& session);

private:
    kanban_db&  m_db;
    long long   m_org_id{0};

    Wt::WContainerWidget* m_list{nullptr};
    Wt::WLineEdit*        m_name_input{nullptr};
    Wt::WColorPicker*     m_color_picker{nullptr};
    Wt::WText*            m_status_msg{nullptr};

    long long             m_editing_id{0};
    Wt::WLineEdit*        m_edit_name{nullptr};
    Wt::WColorPicker*     m_edit_color{nullptr};

    static const std::vector<std::string> k_palette;

    void refresh_list();
    void add_type();
    void start_edit(long long type_id);
    void save_edit(long long type_id);
    void delete_type(long long type_id);
};
