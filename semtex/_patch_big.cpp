void visuals::cache_skeleton_bones()
{
    if (!game::ready() || !menu_vars::skeleton_esp)
        return;

    auto* api = game::studio_api();
    auto** phdr = game::pstudiohdr();
    if (!api || !phdr || !*phdr)
        return;

    auto* ent = api->GetCurrentEntity();
    if (!should_draw_entity(ent))
        return;

    auto* hdr = *phdr;
    auto* bones = reinterpret_cast<mstudiobone_t*>(reinterpret_cast<byte*>(hdr) + hdr->boneindex);
    auto* mats = reinterpret_cast<float(*)[MAXSTUDIOBONES][3][4]>(api->StudioGetBoneTransform());
    if (!bones || !mats)
        return;

    auto& lines = s_bone_lines[ent->index];
    lines.clear();

    for (int i = 0; i < hdr->numbones; ++i)
    {
        if (bones[i].parent == -1)
            continue;

        const int parent = bones[i].parent;
        lines.push_back({
            Vector((*mats)[i][0][3], (*mats)[i][1][3], (*mats)[i][2][3]),
            Vector((*mats)[parent][0][3], (*mats)[parent][1][3], (*mats)[parent][2][3]),
        });
    }
}

void visuals::draw_skeleton_overlay()
{
}

void visuals::draw_skeleton_esp(OSHGui::Drawing::Graphics& gfx)
{
    if (!menu_vars::skeleton_esp || !game::ready() || s_bone_lines.empty())
        return;

    refresh_players();

    for (const auto& [index, segments] : s_bone_lines)
    {
        const auto it = entities::players().find(index);
        if (it != entities::players().end() && !should_draw_player(it->second))
            continue;

        const auto color = (it != entities::players().end())
            ? team_color(it->second)
            : OSHGui::Drawing::Color::FromRGB(180, 180, 180);

        for (const auto& seg : segments)
        {
            Vector2D a{}, b{};
            if (!world::to_screen(seg.a, a) || !world::to_screen(seg.b, b))
                continue;

            gfx.DrawLine(color,
                OSHGui::Drawing::PointF(a.x, a.y),
                OSHGui::Drawing::PointF(b.x, b.y));
        }
    }

    s_bone_lines.clear();
}

void visuals::draw_box_esp(OSHGui::Drawing::Graphics& gfx)
{
    if (!menu_vars::box_esp || !game::ready())
        return;

    refresh_players();

    for (const auto& [index, player] : entities::players())
    {
        (void)index;
        if (!should_draw_player(player))
            continue;

        box_metrics_t box{};
        if (!get_player_box_metrics(player, box))
            continue;

        const auto color = team_color(player);
        const float left   = box.top.x - box.wide_half;
        const float right  = box.top.x + box.wide_half;
        const float top    = box.top.y;
        const float bottom = box.bot.y;

        gfx.DrawLine(color, OSHGui::Drawing::PointF(left, top),    OSHGui::Drawing::PointF(right, top));
        gfx.DrawLine(color, OSHGui::Drawing::PointF(right, top),   OSHGui::Drawing::PointF(right, bottom));
        gfx.DrawLine(color, OSHGui::Drawing::PointF(right, bottom), OSHGui::Drawing::PointF(left, bottom));
        gfx.DrawLine(color, OSHGui::Drawing::PointF(left, bottom), OSHGui::Drawing::PointF(left, top));
    }
}

void visuals::draw_name_esp(OSHGui::Drawing::Graphics& gfx, const OSHGui::Drawing::FontPtr& font)
{
    if (!menu_vars::name_esp || !game::ready())
        return;

    refresh_players();

    for (const auto& [index, player] : entities::players())
    {
        (void)index;
        if (!should_draw_player(player))
            continue;

        box_metrics_t box{};
        if (!get_player_box_metrics(player, box))
            continue;

        const char* name = player.info->name;
        const float text_w = font->GetTextExtent(name ? name : "?");
        const float label_x = box.bot.x - text_w * 0.5f;
        const float label_y = box.bot.y + 2.0f;
        const auto color = team_color(player);
        gfx.DrawString(name ? name : "?", font, color, label_x, label_y);
    }
}