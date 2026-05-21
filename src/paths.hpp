#pragma once

#include <charconv>
#include <format>
#include <optional>
#include <string>
#include <string_view>

namespace paths
{
    // ── Domain prefixes (include trailing slash) ──────────────────────────────
    inline constexpr std::string_view blog_prefix  = "/blog/";
    inline constexpr std::string_view link_prefix  = "/link/";
    inline constexpr std::string_view org_prefix   = "/org/";
    inline constexpr std::string_view team_prefix  = "/team/";
    inline constexpr std::string_view task_prefix  = "/task/";
    inline constexpr std::string_view admin_prefix = "/admin/";

    // ── Common action segments ────────────────────────────────────────────────
    inline constexpr std::string_view list_seg = "list";
    inline constexpr std::string_view view_seg = "view";
    inline constexpr std::string_view edit_seg = "edit";
    inline constexpr std::string_view new_seg  = "new";

    // ── Fixed paths ───────────────────────────────────────────────────────────
    inline constexpr std::string_view login_path         = "/login";
    inline constexpr std::string_view logout_path        = "/logout";
    inline constexpr std::string_view notifications_path = "/notifications";
    inline constexpr std::string_view settings_path      = "/settings";

    // ── Segment helpers ───────────────────────────────────────────────────────

    // Consume the next slash-delimited segment from sv.
    // Strips a leading '/' first if present. Updates sv to the remainder
    // (no leading slash). Returns the consumed segment (empty if sv was empty).
    inline std::string_view take_segment(std::string_view& sv)
    {
        if(!sv.empty() && sv.front() == '/')
            sv.remove_prefix(1);
        const auto slash = sv.find('/');
        if(slash == sv.npos)
        {
            const auto seg = sv;
            sv             = {};
            return seg;
        }
        const auto seg = sv.substr(0, slash);
        sv             = sv.substr(slash + 1);
        return seg;
    }

    // Consume a numeric ID segment. Returns nullopt if absent or non-numeric.
    // On failure, sv is still advanced past the consumed segment.
    inline std::optional<long long> take_id(std::string_view& sv)
    {
        const auto seg = take_segment(sv);
        if(seg.empty())
            return std::nullopt;
        long long   id{};
        const auto* end = seg.data() + seg.size();
        if(std::from_chars(seg.data(), end, id).ptr != end)
            return std::nullopt;
        return id;
    }

    // ── Named path builders ───────────────────────────────────────────────────

    // Blog
    inline std::string blog_list()
    {
        return std::format("{}{}", blog_prefix, list_seg);
    }
    inline std::string blog_view(std::string_view slug)
    {
        return std::format("{}{}/{}", blog_prefix, view_seg, slug);
    }
    inline std::string blog_edit(std::string_view slug)
    {
        return std::format("{}{}/{}", blog_prefix, edit_seg, slug);
    }
    inline std::string blog_new()
    {
        return std::format("{}{}", blog_prefix, new_seg);
    }

    // Link
    inline std::string link_list()
    {
        return std::format("{}{}", link_prefix, list_seg);
    }
    inline std::string link_edit(long long id)
    {
        return std::format("{}{}/{}", link_prefix, edit_seg, id);
    }
    inline std::string link_new()
    {
        return std::format("{}{}", link_prefix, new_seg);
    }

    // Admin — account
    inline std::string account_list()
    {
        return std::format("{}account/{}", admin_prefix, list_seg);
    }
    inline std::string account_new()
    {
        return std::format("{}account/{}", admin_prefix, new_seg);
    }
    inline std::string account_edit(std::string_view username)
    {
        return std::format("{}account/{}/{}", admin_prefix, edit_seg, username);
    }

    // Admin — org
    inline std::string admin_org_list()
    {
        return std::format("{}org/{}", admin_prefix, list_seg);
    }

    // Org
    inline std::string org_view(long long id)
    {
        return std::format("{}{}/{}", org_prefix, view_seg, id);
    }
    inline std::string org_board(long long id)
    {
        return std::format("{}{}/{}/board", org_prefix, view_seg, id);
    }
    inline std::string org_edit(long long id)
    {
        return std::format("{}{}/{}", org_prefix, edit_seg, id);
    }
    inline std::string org_types(long long id)
    {
        return std::format("{}{}/{}/types", org_prefix, view_seg, id);
    }

    // Team
    inline std::string team_kanban(long long id)
    {
        return std::format("{}{}/{}/kanban", team_prefix, view_seg, id);
    }
    inline std::string team_gantt(long long id)
    {
        return std::format("{}{}/{}/gantt", team_prefix, view_seg, id);
    }
    inline std::string team_task_new(long long id)
    {
        return std::format("{}{}/{}/task/new", team_prefix, view_seg, id);
    }
    inline std::string team_archive(long long id)
    {
        return std::format("{}{}/{}/archive", team_prefix, view_seg, id);
    }
    inline std::string team_edit_members(long long id)
    {
        return std::format("{}{}/{}/members", team_prefix, edit_seg, id);
    }
    inline std::string team_edit_settings(long long id)
    {
        return std::format("{}{}/{}/settings", team_prefix, edit_seg, id);
    }

    // Task
    inline std::string task_edit(long long id)
    {
        return std::format("{}{}/{}", task_prefix, edit_seg, id);
    }
}
