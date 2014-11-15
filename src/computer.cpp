#include "computer.h"
#include "game.h"
#include "monster.h"
#include "overmap.h"
#include "output.h"
#include "json.h"
#include "monstergenerator.h"
#include "overmapbuffer.h"
#include "messages.h"
#include "helper.h"
#include <fstream>
#include <string>
#include <sstream>

std::vector<std::string> computer::lab_notes;
int alerts = 0;

/*
in mapgen, generation uses tinymap class which is 2x2 submaps, so all coordinates are relative to a
single 24x24 map tile area.  during gameplay, the reality bubble map is used, which is much larger
so coordinates will NOT be equal between the two.

for computer options that rely on coordinates, assuming coordinates come from json (in the future),
should save the coordinates of the computer, save the coordinates specified in json, calculate offset
between the computer and the target coordinates, then save and process based on the offset.

ex:
 012345
0...... c is computer, t is target
1.c.... c: (1,1) t:(2,4)
2...... save t.x and t.y as as offsets relative to c, or (1,3)
3......
4..t...
5......
*/

compsec_trait::compsec_trait(std::stringstream& stream)
{
    stream >> t;
}

compsec_trait::compsec_trait(JsonObject& jo)
{
    t = jo.get_string("trait");
}

bool compsec_trait::attempt()
{
    return g->u.has_trait(t);
}

std::string compsec_trait::save()
{
    std::stringstream data;
    data << "trait " << t << " ";
    return data.str();
}

compsec_pass::compsec_pass(std::stringstream& stream)
{
    stream >> pass;
}

compsec_pass::compsec_pass(JsonObject& jo)
{
    pass = jo.get_string("pass");
}

bool compsec_pass::attempt()
{
    std::string input = string_input_popup(_("Enter Password"), 256);
    return (input == pass);
}

std::string compsec_pass::save()
{
    std::stringstream data;
    data << "pass " << helper::space_to_underscore(pass) << " ";
    return data.str();
}

compsec_hack::compsec_hack(std::stringstream &stream)
{
    stream >> diff;
}

compsec_hack::compsec_hack(JsonObject& jo)
{
    diff = jo.get_int("diff", 1);
}

bool compsec_hack::attempt()
{
    g->u.practice("computer", 5 + diff * 2, diff + 2);

    int player_roll = g->u.skillLevel("computer");
    if (g->u.int_cur < 8 && one_in(2))
    {
        player_roll -= rng(0, 8 - g->u.int_cur);
    }
    else if (g->u.int_cur > 8 && one_in(3))
    {
        player_roll += rng(0, g->u.int_cur - 8);
    }

    return (dice(player_roll, 6) >= dice(diff, 6));
}

std::string compsec_hack::save()
{
    std::stringstream data;
    data << "hack " << diff << " ";
    return data.str();
}

compsec_item::compsec_item(std::stringstream &stream)
{
    stream >> it >> num;
}

compsec_item::compsec_item(JsonObject& jo)
{
    it = jo.get_string("item");
}

bool compsec_item::attempt()
{
    if(g->u.has_amount(it, num))
    {
        g->u.use_amount(it, num);
        return true;
    }
    else
    {
        return false;
    }
}

std::string compsec_item::save()
{
    std::stringstream data;
    data << "item " << it << " " << num << " ";
    return data.str();
}

compsec_itemat::compsec_itemat(std::stringstream &stream)
{
    stream >> it >> itemx >> itemy;
}

compsec_itemat::compsec_itemat(JsonObject& jo)
{
    it = jo.get_string("item");
    itemx = jo.get_array("loc").next_int();
    itemy = jo.get_array("loc").next_int();
}

bool compsec_itemat::attempt()
{
    //std::vector<item> items = g->m.i_at(c->compx + itemx, c->compy + itemy);
    std::vector<item> items = g->m.i_at(itemx, itemy);

    for(int i = 0; i < items.size(); i++)
    {
        if(items[i].type->id == it)
        {
            return true;
        }
    }
    return false;
}

std::string compsec_itemat::save()
{
    std::stringstream data;
    data << "itemat " << it << " " << itemx << " " << itemy << " ";
    return data.str();
}

compsec_containerat::compsec_containerat(std::stringstream &stream)
{
    stream >> itemx >> itemy >> wt >> soft >> empty;
    if(!empty)
    {
        stream >> it;
    }
}

compsec_containerat::compsec_containerat(JsonObject& jo)
{
    itemx = jo.get_array("loc").next_int();
    itemy = jo.get_array("loc").next_int();
    wt = jo.get_bool("watertight");
    soft = jo.get_bool("software");
    empty = jo.get_bool("empty");
    if(!empty){
        it = jo.get_string("contents");
    }
}

bool compsec_containerat::attempt()
{
    // problem if: not container, req. watertight & item given not,
    // req. empty, & item given not, req. non empty & item given empty
    // req. contents & item wrong contents

    // if watertight, ignore software, vice versa
    int x = itemx;// + c->compx;
    int y = itemy;// + c->compy;
    if(g->m.i_at(x, y).size() == 0)
    {
        return false;
    }
    item itemat = g->m.i_at(x, y)[0];
    if(soft)
    {
        if(itemat.typeId() != "usb_drive")
        {
            return false;
        }
    }
    if(wt)
    {
        if(!itemat.is_watertight_container())
        {
            return false;
        }
    }
    if(empty)
    {
        return (itemat.contents.size() == 0);
    }
    else
    {
        if(itemat.contents[0].typeId() != it)
        {
            return false;
        }
        else
        {
            return true;
        }
    }
}

std::string compsec_containerat::save()
{
    std::stringstream data;
    data << "cnat " << itemx << " " << itemy << " " << wt << " " <<
            soft << " " << empty << " ";
    if(!empty)
    {
        data << it << " ";
    }
    return data.str();
}

compact_chter::compact_chter(std::stringstream &stream)
{
    stream >> terx >> tery >> ter;
}

compact_chter::compact_chter(JsonObject& jo)
{
    ter = jo.get_string("terrain");
    terx = jo.get_array("loc").next_int();
    tery = jo.get_array("loc").next_int();
}

void compact_chter::go()
{
    //g->m.ter_set(c->compx + terx, c->compy + tery, ter);
    g->m.ter_set(terx, tery, ter);
}

std::string compact_chter::save()
{
    std::stringstream data;
    data << "cht " << terx << " " << tery << " " << ter << " ";
    return data.str();
}

compact_msg::compact_msg(std::stringstream &stream)
{
    stream >> msg;
    msg = helper::swap_char(helper::underscore_to_space(msg), '|', '\n');
}

compact_msg::compact_msg(JsonObject &jo)
{
    msg = jo.get_string("message");
}

void compact_msg::go()
{
    c->reset_terminal();
    c->print_text(msg.c_str());
}

std::string compact_msg::save()
{
    std::stringstream data;
    data << "msg " << helper::swap_char(helper::space_to_underscore(msg), '\n', '|') << " ";
    return data.str();
}

compact_chlvl::compact_chlvl(std::stringstream &stream)
{
    stream >> z;
}

compact_chlvl::compact_chlvl(JsonObject &jo)
{
    z = jo.get_int("z");
}

void compact_chlvl::go()
{
    g->vertical_move(z, true);
}

std::string compact_chlvl::save()
{
    std::stringstream data;
    data << "chz " << z << " ";
    return data.str();
}

compact_noise::compact_noise(std::stringstream &stream)
{
    stream >> vol >> desc;
    desc = helper::underscore_to_space(desc);
}

compact_noise::compact_noise(JsonObject &jo)
{
    vol = jo.get_int("volume");
    desc = jo.get_string("message");
}

void compact_noise::go()
{
    // maybe dont use the users position?
    // use consoles, or specify location
    g->sound(g->u.posx, g->u.posy, vol, desc);
}

std::string compact_noise::save()
{
    std::stringstream data;
    data << "snd " << vol << " " << helper::space_to_underscore(desc) << " ";
    return data.str();
}

compact_mon::compact_mon(std::stringstream &stream)
{
    stream >> monx >> mony >> mon;
}

compact_mon::compact_mon(JsonObject &jo)
{
    mon = jo.get_string("monster");
    monx = jo.get_array("loc").next_int();
    mony = jo.get_array("loc").next_int();
}

void compact_mon::go()
{
    monster newmon(GetMType(mon));
    //newmon.spawn(c->compx + monx, c->compy + mony);
    newmon.spawn(monx, mony);
    g->add_zombie(newmon);
}

std::string compact_mon::save()
{
    std::stringstream data;
    data << "mon " << monx << " " << mony << " " << mon << " ";
    return data.str();
}

compact_item::compact_item(std::stringstream &stream)
{
    stream >> itemx >> itemy >> it;
}

compact_item::compact_item(JsonObject &jo)
{
    it = jo.get_string("item");
    itemx = jo.get_array("loc").next_int();
    itemy = jo.get_array("loc").next_int();
}

void compact_item::go()
{
    //g->m.spawn_item(c->compx + itemx, c->compy + itemy, it, 0, 1);
    g->m.spawn_item(itemx, itemy, it, 0, 1);
}

std::string compact_item::save()
{
    std::stringstream data;
    data << "itm " << itemx << " " << itemy << " " << it << " ";
    return data.str();
}

compact_fill::compact_fill(std::stringstream &stream)
{
    stream >> itemx >> itemy >> it >> amt;
}

compact_fill::compact_fill(JsonObject &jo)
{
    it = jo.get_string("item");
    amt = jo.get_int("amount");
    itemx = jo.get_array("loc").next_int();
    itemy = jo.get_array("loc").next_int();
}

void compact_fill::go()
{
    //item *container = &(g->m.i_at(c->compx + itemx, c->compy + itemy)[0]);
    item *container = &(g->m.i_at(itemx, itemy)[0]);
    item contents(it, calendar::turn);

    if(contents.is_software())
    {
        container->put_in(contents);
    }
    else if(container->is_container())
    {
        container->put_in(contents);
    }
}

std::string compact_fill::save()
{
    std::stringstream data;
    data << "fil " << itemx << " " << itemy << " " << it << " " << amt << " ";
    return data.str();
}

compact_map::compact_map(std::stringstream &stream)
{
    int numtypes;
    stream >> rad >> z >> numtypes;
    for(int i = 0; i < numtypes; i++)
    {
        std::string val;
        stream >> val;
        types.push_back(val);
    }
}

compact_map::compact_map(JsonObject &jo)
{
    rad = jo.get_int("radius");
    z = jo.get_int("z");
    JsonArray typeArray = jo.get_array("types");
    while(typeArray.has_more()){
        types.push_back(typeArray.next_string());
    }
}

void compact_map::go()
{
    int minx = int((g->levx + int(MAPSIZE / 2)) / 2) - rad;
    int maxx = int((g->levx + int(MAPSIZE / 2)) / 2) + rad;
    int miny = int((g->levy + int(MAPSIZE / 2)) / 2) - rad;
    int maxy = int((g->levy + int(MAPSIZE / 2)) / 2) + rad;
    if (minx < 0)
    {
        minx = 0;
    }
    if (maxx >= OMAPX)
    {
        maxx = OMAPX - 1;
    }
    if (miny < 0)
    {
        miny = 0;
    }
    if (maxy >= OMAPY)
    {
        maxy = OMAPY - 1;
    }

    if(types.size() == 0)
    {
        for (int i = minx; i <= maxx; i++)
        {
            for (int j = miny; j <= maxy; j++)
            {
                g->cur_om->seen(i, j, z) = true;
            }
        }
    }
    else
    {
        for (int i = minx; i <= maxx; i++)
        {
            for (int j = miny; j <= maxy; j++)
            {
                for(int k = 0; k < types.size(); k++)
                {
                    const tripoint center = g->om_global_location();
                    const oter_id &oter = overmap_buffer.ter(center.x + i, center.y + j, center.z);
                    if (is_ot_type(types.at(k), oter)) {
                        overmap_buffer.set_seen(center.x + i, center.y + j, center.z, true);
                    }
                }
            }
        }
    }
}

std::string compact_map::save()
{
    std::stringstream data;
    data << "map " << rad << " " << z << " " << types.size() << " ";
    for(int i = 0; i < types.size(); i++)
    {
        data << types[i] << " ";
    }
    return data.str();
}

compact_trap::compact_trap(std::stringstream &stream)
{
    stream >> trapx >> trapy >> t;
}

compact_trap::compact_trap(JsonObject &jo)
{
    if(trapmap.find(jo.get_string("trap")) != trapmap.end()){
        t = trapmap[(jo.get_string("trap"))];
    } else {
        t = tr_null;
    }

    trapx = jo.get_array("loc").next_int();
    trapy = jo.get_array("loc").next_int();
}

void compact_trap::go()
{
    //g->m.add_trap(trapx+c->compx, trapy+c->compy, (trap_id)t);
    g->m.add_trap(trapx, trapy, (trap_id)t);
}

std::string compact_trap::save()
{
    std::stringstream data;
    data << "trp " << trapx << " " << trapy << " " << t << " ";
    return data.str();
}

compact_remtrap::compact_remtrap(std::stringstream &stream)
{
    stream >> trapx >> trapy;
}

compact_remtrap::compact_remtrap(JsonObject &jo)
{
    trapx = jo.get_array("loc").next_int();
    trapy = jo.get_array("loc").next_int();
}

void compact_remtrap::go()
{
    //g->m.remove_trap(trapx + c->compx, trapy + c->compy);
    g->m.remove_trap(trapx, trapy);
}

std::string compact_remtrap::save()
{
    std::stringstream data;
    data << "rmt " << trapx << " " << trapy << " ";
    return data.str();
}

compact_field::compact_field(std::stringstream &stream)
{
    stream >> fieldx >> fieldy >> f >> den;
}

compact_field::compact_field(JsonObject &jo)
{
    f = field_from_ident(jo.get_string("field"));
    den = jo.get_int("density");
    fieldx = jo.get_array("loc").next_int();
    fieldy = jo.get_array("loc").next_int();
}

void compact_field::go()
{
    g->m.add_field(fieldx, fieldy, (field_id)f, den);
}

std::string compact_field::save()
{
    std::stringstream data;
    data << "fld " << fieldx << " " << fieldy << " " << f << " " << den << " ";
    return data.str();
}

compact_remfield::compact_remfield(std::stringstream &stream)
{
    stream >> fieldx >> fieldy >> f;
}

compact_remfield::compact_remfield(JsonObject &jo)
{
    f = field_from_ident(jo.get_string("field"));
    fieldx = jo.get_array("loc").next_int();
    fieldy = jo.get_array("loc").next_int();
}


void compact_remfield::go()
{
    //g->m.remove_field(fieldx + c->compx, fieldy + c->compy, (field_id)f);
    g->m.remove_field(fieldx, fieldy, (field_id)f);
}

std::string compact_remfield::save()
{
    std::stringstream data;
    data << "rmf " << fieldx << " " << fieldy << " " << f << " ";
    return data.str();
}

compact_exp::compact_exp(std::stringstream &stream)
{
    stream >> expx >> expy >> pwr >> shrap >> fire;
}

compact_exp::compact_exp(JsonObject &jo)
{
    pwr = jo.get_int("power");
    shrap = jo.get_int("shrapnel");
    fire = jo.get_bool("fiery");
    expx = jo.get_array("loc").next_int();
    expy = jo.get_array("loc").next_int();
}

void compact_exp::go()
{
    //g->explosion(expx+c->compx, expy+c->compx, pwr, shrap, fire);
    g->explosion(expx, expy, pwr, shrap, fire);
}

std::string compact_exp::save()
{
    std::stringstream data;
    data << "exp " << expx << " " << expy << " " << pwr << " " << shrap << " " << fire << " ";
    return data.str();
}

compact_hurt::compact_hurt(std::stringstream &stream)
{
    stream >> min >> max;
}

compact_hurt::compact_hurt(JsonObject &jo)
{
    min = jo.get_array("damage").next_int();
    max = jo.get_array("damage").next_int();
}

void compact_hurt::go()
{
    g->u.hurtall(rng(min, max));
}

std::string compact_hurt::save()
{
    std::stringstream data;
    data << "hrt " << min << " " << max << " ";
    return data.str();
}

compact_killmon::compact_killmon(std::stringstream &stream)
{
    stream >> tlx >> tly >> brx >> bry;
}

compact_killmon::compact_killmon(JsonObject &jo)
{
    tlx = jo.get_array("topleft").next_int();
    tly = jo.get_array("topleft").next_int();
    brx = jo.get_array("bottomright").next_int();
    bry = jo.get_array("bottomright").next_int();
}

void compact_killmon::go()
{
    //for(int i = tlx + c->compx; i <= brx + c->compx; i++)
    for(int i = tlx; i <= brx; i++)
    {
        //for(int j = tly + c->compy; j <= bry + c->compy; j++)
        for(int j = tly; j <= bry; j++)
        {
            int mondex = g->mon_at(i, j);
            if(mondex != -1)
            {
                // hopefully the mondex check is enough to ensure that critter_at is not null
                g->critter_at(i, j)->die(nullptr);
            }
        }
    }
}

std::string compact_killmon::save()
{
    std::stringstream data;
    data << "kil " << tlx << " " << tly << " " << brx << " " << bry << " ";
    return data.str();
}

compact_disease::compact_disease(std::stringstream &stream)
{
    stream >> d >> dur;
}

compact_disease::compact_disease(JsonObject &jo)
{
    d = jo.get_string("disease");
    dur = jo.get_int("duration");
}

// going to assume just basic diseases for now, disease/duration only
void compact_disease::go()
{
    g->u.add_disease((dis_type)d, dur);
}

std::string compact_disease::save()
{
    std::stringstream data;
    data << "dis " << d << " " << dur << " ";
    return data.str();
}

compact_pain::compact_pain(std::stringstream &stream)
{
    stream >> min >> max;
}

compact_pain::compact_pain(JsonObject &jo)
{
    min = jo.get_array("pain").next_int();
    max = jo.get_array("pain").next_int();
}

void compact_pain::go()
{
    g->u.pain += rng(min, max);
}

std::string compact_pain::save()
{
    std::stringstream data;
    data << "pai " << min << " " << max << " ";
    return data.str();
}

compact_event::compact_event(std::stringstream &stream)
{
    stream >> type >> turn >> fac >> eventx >> eventy;
}

compact_event::compact_event(JsonObject &jo)
{
    type = jo.get_int("type");
    turn = jo.get_int("turn");
    fac = jo.get_int("faction", -1);
    if(jo.has_array("loc")){
        eventx = jo.get_array("loc").next_int();
        eventy = jo.get_array("loc").next_int();
    } else {
        eventx = INT_MIN;
        eventy = INT_MIN;
    }
}

void compact_event::go()
{
    g->add_event((event_type)type, calendar::turn + turn, fac, eventx, eventy);
}

std::string compact_event::save()
{
    std::stringstream data;
    data << "evt " << type << " " << turn << " " << fac << " " << eventx << " " << eventy << " ";
    return data.str();
}

void compact_cascade::go()
{
    if (!c->query_bool(_("WARNING: Resonance cascade carries severe risk!  Continue?")))
    {
        return;
    }
    g->u.add_memorial_log(_("Caused a resonance cascade."), _("Caused a resonance cascade."));
    std::vector<point> cascade_points;
    for (int i = g->u.posx - 10; i <= g->u.posx + 10; i++)
    {
        for (int j = g->u.posy - 10; j <= g->u.posy + 10; j++)
        {
            if (g->m.ter(i, j) == t_radio_tower)
            {
                cascade_points.push_back(point(i, j));
            }
        }
    }
    if (cascade_points.empty())
    {
        g->resonance_cascade(g->u.posx, g->u.posy);
    }
    else
    {
        point p = cascade_points[rng(0, cascade_points.size() - 1)];
        g->resonance_cascade(p.x, p.y);
    }
}

std::string compact_cascade::save()
{
    return "rcs ";
}

void compact_nuke::go()
{
/*
    point target = g->cur_om->draw_overmap_in_window(c->w_terminal, 0);
//    WINDOW* w = newwin(getmaxy(c->w_border)-2, 24,
//                       getbegy(c->w_border)+1, getbegx(c->w_border)+1);
//    for(int i = 0; i < getmaxy(c->w_border)-2; i++)
//    {
//        mvwputch(w, 24, i, c_yellow, LINE_XOXO);
//    }
    wrefresh(c->w_border);
    wrefresh(c->w_terminal);

    c->reset_terminal();
    //c->print_line("You picked a %s.", oterlist[g->cur_om->ter(target.x, target.y, 0)].name.c_str());

    if (target.x == -1)
    {
        g->add_msg(_("Target acquisition canceled"));
        return;
    }
    if(query_yn(_("Confirm nuclear missile launch.")))
    {
        g->add_msg(_("Nuclear missile launched!"));
    }
    else
    {
        g->add_msg(_("Nuclear missile launch aborted."));
        return;
    }
    g->refresh_all();

    //Put some smoke gas and explosions at the nuke location.
    for(int i= g->u.posx +8; i < g->u.posx +15; i++)
    {
        for(int j= g->u.posy +3; j < g->u.posy +12; j++)
            if(!one_in(4))
            {
                g->m.add_field(NULL, i+rng(-2,2), j+rng(-2,2), fd_smoke, rng(1,9));
            }
    }

    g->explosion(g->u.posx +10, g->u.posx +21, 200, 0, true); //Only explode once. But make it large.

    //...ERASE MISSILE, OPEN SILO, DISABLE COMPUTER
    // For each level between here and the surface, remove the missile
    for (int level = g->levz; level <= 0; level++)
    {
        map tmpmap(&g->traps);
        tmpmap.load(g, g->levx, g->levy, level, false);

        if(level < 0)
        {
            tmpmap.translate(t_missile, t_hole);
        }
        else if(level == 0)
        {
            tmpmap.translate(t_metal_floor, t_hole);
        }
        tmpmap.save(g->cur_om, g->turn, g->levx, g->levy, level);
    }

    g->u.add_memorial_log(_("Launched a nuke at a %s."),
            oterlist[g->cur_om->ter(target.x, target.y, 0)].name.c_str());
    for(int x = target.x - 2; x <= target.x + 2; x++)
    {
        for(int y = target.y -  2; y <= target.y + 2; y++)
        {
            // make that nice circular shape
            if(!((x==target.x-2 && y==target.y-2) ||
                 (x==target.x-2 && y==target.y+2) ||
                 (x==target.x+2 && y==target.y+2) ||
                 (x==target.x+2 && y==target.y-2)))
            {
                g->nuke(x, y);
            }
        }
    }

    for(int i = 0; i < c->compopts.size(); i++)
    {
        for(int j = 0; j < c->compopts[i].actions.size(); j++)
        {
            if(c->compopts[i].actions[j] == this)
            {
                c->compopts[i].actions.erase(c->compopts[i].actions.begin() + j);
                delete this;
                c->compopts.erase(c->compopts.begin() + i);
            }
        }
    }

    touchwin(c->w_border);
    wrefresh(c->w_border);
    touchwin(c->w_terminal);
    wrefresh(c->w_terminal);
*/
}

std::string compact_nuke::save()
{
    return "nuk ";
}

compopt::compopt(std::stringstream &stream)
{
    int numsec, numact, numfail;
    stream >> prompt >> numsec ;
    prompt = helper::underscore_to_space(prompt);
    for(int k = 0; k < numsec; k++)
    {
        std::string security;
        stream >> security;
        if(security == "pass")
        {
            add_security(new compsec_pass(stream));
        }
        if(security == "item")
        {
            add_security(new compsec_item(stream));
        }
        if(security == "hack")
        {
            add_security(new compsec_hack(stream));
        }
        if(security == "itemat")
        {
            add_security(new compsec_itemat(stream));
        }
        if(security == "trait")
        {
            add_security(new compsec_trait(stream));
        }
    }
    stream >> numact;
    for(int i = 0; i < numact; i++)
    {
        std::string action;
        stream >> action;
        if(action == "cht")
        {
            add_action(new compact_chter(stream));
        }
        if(action == "msg")
        {
            add_action(new compact_msg(stream));
        }
        if(action == "map")
        {
            add_action(new compact_map(stream));
        }
        if(action == "itm")
        {
            add_action(new compact_item(stream));
        }
        if(action == "mon")
        {
            add_action(new compact_mon(stream));
        }
        if(action == "chz")
        {
            add_action(new compact_chlvl(stream));
        }
        if(action == "snd")
        {
            add_action(new compact_noise(stream));
        }
        if(action == "kil")
        {
            add_action(new compact_killmon(stream));
        }
        if(action == "trp")
        {
            add_action(new compact_trap(stream));
        }
        if(action == "fld")
        {
            add_action(new compact_field(stream));
        }
        if(action == "exp")
        {
            add_action(new compact_exp(stream));
        }
        if(action == "hrt")
        {
            add_action(new compact_hurt(stream));
        }
        if(action == "rmf")
        {
            add_action(new compact_remfield(stream));
        }
        if(action == "rmt")
        {
            add_action(new compact_remtrap(stream));
        }
    }
    stream >> numfail;
    for(int j = 0; j < numfail; j++)
    {
        std::string action;
        stream >> action;
        if(action == "cht")
        {
            add_failure(new compact_chter(stream));
        }
        if(action == "msg")
        {
            add_failure(new compact_msg(stream));
        }
        if(action == "map")
        {
            add_failure(new compact_map(stream));
        }
        if(action == "itm")
        {
            add_failure(new compact_item(stream));
        }
        if(action == "mon")
        {
            add_failure(new compact_mon(stream));
        }
        if(action == "chz")
        {
            add_failure(new compact_chlvl(stream));
        }
        if(action == "snd")
        {
            add_failure(new compact_noise(stream));
        }
        if(action == "kil")
        {
            add_failure(new compact_killmon(stream));
        }
        if(action == "trp")
        {
            add_failure(new compact_trap(stream));
        }
        if(action == "fld")
        {
            add_failure(new compact_field(stream));
        }
        if(action == "exp")
        {
            add_failure(new compact_exp(stream));
        }
        if(action == "hrt")
        {
            add_failure(new compact_hurt(stream));
        }
        if(action == "rmf")
        {
            add_failure(new compact_remfield(stream));
        }
        if(action == "rmt")
        {
            add_failure(new compact_remtrap(stream));
        }
    }
}

// made from assumption on json structure at
// https://github.com/vache/Cataclysm-DDA/issues/7
compopt::compopt(JsonObject& jo)
{
    prompt = jo.get_string("prompt");
    JsonArray secArray = jo.get_array("security");
    while(secArray.has_more()){
        JsonObject secObject = secArray.next_object();
        std::string type = secObject.get_string("security");

        if(type == "pass"){
            add_security(new compsec_pass(secObject));
        } else if(type == "item"){
            add_security(new compsec_item(secObject));
        } else if(type == "hack"){
            add_security(new compsec_hack(secObject));
        } else if(type == "itemat"){
            add_security(new compsec_itemat(secObject));
        } else if(type == "contat"){
            add_security(new compsec_containerat(secObject));
        }
    }

    JsonArray successArray = jo.get_array("actions");
    while(successArray.has_more()){
        JsonObject successObject = successArray.next_object();
        std::string type = successObject.get_string("action");

        if(type == "cht"){
            add_action(new compact_chter(successObject));
        } else if(type == "msg"){
            add_action(new compact_msg(successObject));
        } else if(type == "map"){
            add_action(new compact_map(successObject));
        } else if(type == "itm"){
            add_action(new compact_item(successObject));
        } else if(type == "mon"){
            add_action(new compact_mon(successObject));
        } else if(type == "chz"){
            add_action(new compact_chlvl(successObject));
        } else if(type == "snd"){
            add_action(new compact_noise(successObject));
        } else if(type == "kil"){
            add_action(new compact_killmon(successObject));
        } else if(type == "trp"){
            add_action(new compact_trap(successObject));
        } else if(type == "fld"){
            add_action(new compact_field(successObject));
        } else if(type == "exp"){
            add_action(new compact_exp(successObject));
        } else if(type == "hrt"){
            add_action(new compact_hurt(successObject));
        } else if(type == "rmf"){
            add_action(new compact_remfield(successObject));
        } else if(type == "rmt"){
            add_action(new compact_remtrap(successObject));
        }
    }

    JsonArray failureArray = jo.get_array("failures");
    while(failureArray.has_more()){
        JsonObject failureObject = failureArray.next_object();
        std::string type = failureObject.get_string("action");

        if(type == "cht"){
            add_failure(new compact_chter(failureObject));
        } else if(type == "msg"){
            add_failure(new compact_msg(failureObject));
        } else if(type == "map"){
            add_failure(new compact_map(failureObject));
        } else if(type == "itm"){
            add_failure(new compact_item(failureObject));
        } else if(type == "mon"){
            add_failure(new compact_mon(failureObject));
        } else if(type == "chz"){
            add_failure(new compact_chlvl(failureObject));
        } else if(type == "snd"){
            add_failure(new compact_noise(failureObject));
        } else if(type == "kil"){
            add_failure(new compact_killmon(failureObject));
        } else if(type == "trp"){
            add_failure(new compact_trap(failureObject));
        } else if(type == "fld"){
            add_failure(new compact_field(failureObject));
        } else if(type == "exp"){
            add_failure(new compact_exp(failureObject));
        } else if(type == "hrt"){
            add_failure(new compact_hurt(failureObject));
        } else if(type == "rmf"){
            add_failure(new compact_remfield(failureObject));
        } else if(type == "rmt"){
            add_failure(new compact_remtrap(failureObject));
        }
    }
}

compopt::~compopt()
{
    for(auto it = actions.begin(); it != actions.end(); ++it){
        delete *it;
    }
    for(auto it = failures.begin(); it != failures.end(); ++it){
        delete *it;
    }
    for(auto it = security.begin(); it != security.end(); ++it){
        delete *it;
    }
}

void compopt::go()
{
    if(security.size() > 0)
    {
        int s = security.size();
        bool succeed = true;
        for(int i = 0; i < s; i++)
        {
            succeed = succeed && security[i]->attempt();
            if(!succeed)
            {
                break;
            }
        }

        if(!succeed)
        {
            int f = failures.size();
            for(int i = 0; i < f; i++)
            {
                failures[i]->go();
            }
            return;
        }
    }

    int a = actions.size();
    for(int i = 0; i < a; i++)
    {
        actions[i]->go();
    }
}

void compopt::add_security(compsec *measure)
{
    security.push_back(measure);
}

void compopt::add_action(compact *action)
{
    actions.push_back(action);
}

void compopt::add_failure(compact *action)
{
    failures.push_back(action);
}

std::string compopt::save()
{
    std::stringstream data;
    data << helper::space_to_underscore(prompt) << " " << security.size() << " ";
    for(int i = 0; i < security.size(); i++)
    {
        data << security[i]->save();
    }
    data << actions.size() << " ";
    for(int i = 0; i < actions.size(); i++)
    {
        data << actions[i]->save();
    }
    data << failures.size() << " ";
    for(int i = 0; i < failures.size(); i++)
    {
        data << failures[i]->save();
    }
    return data.str();
}

void compopt::set_computer(computer *comp)
{
    c = comp;
    for(int i = 0; i < security.size(); i++)
    {
        security[i]->set_computer(comp);
    }
    for(int i = 0; i < actions.size(); i++)
    {
        actions[i]->set_computer(comp);
    }
    for(int i = 0; i < failures.size(); i++)
    {
        failures[i]->set_computer(comp);
    }
}

computer::computer(): name(DEFAULT_COMPUTER_NAME)
{
    security = 0;
    w_terminal = NULL;
    w_border = NULL;
    mission_id = -1;
}

computer::computer(std::string Name, int Security): name(Name)
{
    security = Security;
    w_terminal = NULL;
    w_border = NULL;
    mission_id = -1;
}

computer::~computer()
{
    if (w_terminal != NULL) {
        delwin(w_terminal);
    }
    if (w_border != NULL) {
        delwin(w_border);
    }
}

computer &computer::operator=(const computer &rhs)
{
    security = rhs.security;
    name = rhs.name;
    mission_id = rhs.mission_id;
    options = rhs.options;
    failures = rhs.failures;
    w_terminal = NULL;
    w_border = NULL;
    return *this;
}

void computer::set_security(int Security)
{
    security = Security;
}

void computer::add_option(std::string opt_name, computer_action action,
                          int Security)
{
    options.push_back(computer_option(opt_name, action, Security));
}

void computer::add_failure(computer_failure failure)
{
    failures.push_back(failure);
}

void computer::shutdown_terminal()
{
    // So yeah, you can reset the term by logging off.
    // Otherwise, it's persistent across all terms.
    // Decided to go easy on people for now.
    alerts = 0;
    werase(w_terminal);
    delwin(w_terminal);
    w_terminal = NULL;
    werase(w_border);
    delwin(w_border);
    w_border = NULL;
}

void computer::use()
{
    if (w_border == NULL) {
        w_border = newwin(FULL_SCREEN_HEIGHT, FULL_SCREEN_WIDTH,
                          (TERMY > FULL_SCREEN_HEIGHT) ? (TERMY - FULL_SCREEN_HEIGHT) / 2 : 0,
                          (TERMX > FULL_SCREEN_WIDTH) ? (TERMX - FULL_SCREEN_WIDTH) / 2 : 0);
    }
    if (w_terminal == NULL) {
        w_terminal = newwin(getmaxy(w_border) - 2, getmaxx(w_border) - 2,
                            getbegy(w_border) + 1, getbegx(w_border) + 1);
    }
    draw_border(w_border);
    wrefresh(w_border);

    // Login
    print_line(_("Logging into %s..."), name.c_str());

    bool legacy = (this->compopts.size() == 0);
    // This is how we process legacy computers
    if(legacy){
        if (security > 0) {
            print_error(_("ERROR!  Access denied!"));
            switch (query_ynq(_("Bypass security?"))) {
            case 'q':
            case 'Q':
                shutdown_terminal();
                return;

            case 'n':
            case 'N':
                query_any(_("Shutting down... press any key."));
                shutdown_terminal();
                return;

            case 'y':
            case 'Y':
                if (!hack_attempt(&(g->u))) {
                    if (failures.empty()) {
                        query_any(_("Maximum login attempts exceeded. Press any key..."));
                        shutdown_terminal();
                        return;
                    }
                    activate_random_failure();
                    shutdown_terminal();
                    return;
                } else { // Successful hack attempt
                    security = 0;
                    query_any(_("Login successful.  Press any key..."));
                    reset_terminal();
                }
            }
        } else { // No security
            query_any(_("Login successful.  Press any key..."));
            reset_terminal();
        }

        // Main computer loop
        while(true) {
            //reset_terminal();
            size_t options_size = options.size();
            print_newline();
            print_line("%s - %s", name.c_str(), _("Root Menu"));
            for (size_t i = 0; i < options_size; i++) {
                print_line("%d - %s", i + 1, options[i].name.c_str());
            }
            print_line("Q - %s", _("Quit and shut down"));
            print_newline();

            char ch;
            do {
                ch = getch();
            } while (ch != 'q' && ch != 'Q' && (ch < '1' || ch - '1' >= (char)options_size));
            if (ch == 'q' || ch == 'Q') {
                break; // Exit from main computer loop
            } else { // We selected an option other than quit.
                ch -= '1'; // So '1' -> 0; index in options.size()
                computer_option current = options[ch];
                // Once you trip the security, you have to roll every time you want to do something
                if ((current.security + (alerts)) > 0) {
                    print_error(_("Password required."));
                    if (query_bool(_("Hack into system?"))) {
                        if (!hack_attempt(&(g->u), current.security)) {
                            activate_random_failure();
                            shutdown_terminal();
                            return;
                        } else {
                            // Successfully hacked function
                            options[ch].security = 0;
                            activate_function(current.action);
                        }
                    }
                } else { // No need to hack, just activate
                    activate_function(current.action);
                }
                reset_terminal();
            } // Done processing a selected option.
        }
    } else {
        // end legacy computer processing
        // new computer processing
        // Main computer loop
        for (bool InUse = true; InUse; )
        {
            print_newline();
            print_line("%s - %s", name.c_str(), _("Root Menu"));
            for (int i = 0; i < compopts.size(); i++)
            {
                print_line("%d - %s", i + 1, compopts[i].prompt.c_str());
            }
            print_line("Q - %s", _("Quit and shut down"));
            print_newline();

            char ch;
            do
            {
                ch = getch();
            }
            while (ch != 'q' && ch != 'Q' && (ch < '1' || ch - '1' >= compopts.size()));

            if (ch == 'q' || ch == 'Q')
            {
                InUse = false;
            }
            else   // We selected an option other than quit.
            {
                ch -= '1'; // So '1' -> 0; index in options.size()
                compopts[ch].go();
                query_any(_("Press any key to continue..."));
                reset_terminal();
            } // Done processing a selected option.
        }
    }
    // end new computer procesing

    shutdown_terminal(); // This should have been done by now, but just in case.
}

bool computer::hack_attempt(player *p, int Security)
{
    if (Security == -1) {
        Security = security;    // Set to main system security if no value passed
    }
    const int hack_skill = p->get_skill_level( "computer" );

    // Every time you dig for lab notes, (or, in future, do other suspicious stuff?)
    // +2 dice to the system's hack-resistance
    // So practical max files from a given terminal = 5, at 10 Computer
    if (alerts > 0) {
        Security += (alerts * 2);
    }

    p->moves -= 10 * (5 + Security * 2) / std::max( 1, hack_skill + 1 );
    p->practice( "computer", 5 + Security * 2 );
    int player_roll = hack_skill;
    if (p->int_cur < 8 && one_in(2)) {
        player_roll -= rng(0, 8 - p->int_cur);
    } else if (p->int_cur > 8 && one_in(3)) {
        player_roll += rng(0, p->int_cur - 8);
    }

    return (dice(player_roll, 6) >= dice(Security, 6));
}

std::string computer::save_data()
{
    std::stringstream data;
    std::string savename = name; // Replace " " with "_"
    size_t found = savename.find(" ");
    while (found != std::string::npos) {
        savename.replace(found, 1, "_");
        found = savename.find(" ");
    }
    data << savename << " " << security << " " << mission_id << " " <<
         options.size() << " ";
    for (std::vector<computer_option>::iterator it = options.begin();
         it != options.end(); ++it) {
        savename = it->name;
        found = savename.find(" ");
        while (found != std::string::npos) {
            savename.replace(found, 1, "_");
            found = savename.find(" ");
        }
        data << savename << " " << int(it->action) << " " << it->security << " ";
    }
    data << failures.size() << " ";
    for (std::vector<computer_failure>::iterator it = failures.begin();
         it != failures.end(); ++it) {
        data << int(*it) << " ";
    }

    return data.str();
}

void computer::load_data(std::string data)
{
    options.clear();
    failures.clear();
    std::stringstream dump;
    dump << data;

    // Pull in name and security
    dump >> name >> security >> mission_id;
    size_t found = name.find("_");
    while (found != std::string::npos) {
        name.replace(found, 1, " ");
        found = name.find("_");
    }

    // Pull in options
    int optsize;
    dump >> optsize;
    for (int n = 0; n < optsize; n++) {
        std::string tmpname;
        int tmpaction, tmpsec;
        dump >> tmpname >> tmpaction >> tmpsec;
        size_t tmp_found = tmpname.find("_");
        while (tmp_found != std::string::npos) {
            tmpname.replace(tmp_found, 1, " ");
            tmp_found = tmpname.find("_");
        }
        add_option(tmpname, computer_action(tmpaction), tmpsec);
    }

    // Pull in failures
    int failsize, tmpfail;
    dump >> failsize;
    for (int n = 0; n < failsize; n++) {
        dump >> tmpfail;
        failures.push_back(computer_failure(tmpfail));
    }
}

void computer::activate_function(computer_action action)
{
    // Token move cost for any action, if an action takes longer decrement moves further.
    g->u.moves -= 30;
    switch (action) {

    case COMPACT_NULL: // Unknown action.
    case NUM_COMPUTER_ACTIONS: // Suppress compiler warning [-Wswitch]
        break;

    case COMPACT_OPEN:
        g->m.translate_radius(t_door_metal_locked, t_floor, 25.0, g->u.posx, g->u.posy);
        query_any(_("Doors opened.  Press any key..."));
        break;

    //LOCK AND UNLOCK are used to build more complex buildings
    // that can have multiple doors that can be locked and be
    // unlocked by different computers.
    //Simply uses translate_radius which take a given radius and
    // player position to determine which terrain tiles to edit.
    case COMPACT_LOCK:
        g->m.translate_radius(t_door_metal_c, t_door_metal_locked, 8.0, g->u.posx, g->u.posy);
        query_any(_("Lock enabled.  Press any key..."));
        break;

    case COMPACT_UNLOCK:
        g->m.translate_radius(t_door_metal_locked, t_door_metal_c, 8.0, g->u.posx, g->u.posy);
        query_any(_("Lock disabled.  Press any key..."));
        break;

    //Toll is required for the church computer/mechanism to function
    case COMPACT_TOLL:
        //~ the sound of a church bell ringing
        g->sound(g->u.posx, g->u.posy, 120, _("Bohm... Bohm... Bohm..."));
        break;

    case COMPACT_SAMPLE:
        g->u.moves -= 30;
        for (int x = 0; x < SEEX * MAPSIZE; x++) {
            for (int y = 0; y < SEEY * MAPSIZE; y++) {
                if (g->m.ter(x, y) == t_sewage_pump) {
                    for (int x1 = x - 1; x1 <= x + 1; x1++) {
                        for (int y1 = y - 1; y1 <= y + 1; y1++ ) {
                            if (g->m.furn(x1, y1) == f_counter) {
                                bool found_item = false;
                                for (std::vector<item>::iterator it = g->m.i_at(x1, y1).begin();
                                     it != g->m.i_at(x1, y1).end(); ++it) {
                                    if (it->is_container()) {
                                        item sewage = item("sewage", calendar::turn);
                                        it_container *container = dynamic_cast<it_container *>(it->type);
                                        it_comest    *comest    = dynamic_cast<it_comest *>(sewage.type);
                                        long maxCharges = container->contains * comest->charges;

                                        if (it->contents.empty()) {
                                            it->put_in(sewage);
                                            found_item = true;
                                            break;
                                        } else {
                                            if (it->contents[0].type->id == sewage.type->id) {
                                                if (it->contents[0].charges < maxCharges) {
                                                    it->contents[0].charges += comest->charges;
                                                    found_item = true;
                                                    break;
                                                }
                                            }
                                        }
                                    }
                                }
                                if (!found_item) {
                                    item sewage("sewage", calendar::turn);
                                    g->m.add_item_or_charges(x1, y1, sewage);
                                }
                            }
                        }
                    }
                }
            }
        }
        break;

    case COMPACT_RELEASE:
        g->u.add_memorial_log(pgettext("memorial_male", "Released subspace specimens."),
                              pgettext("memorial_female", "Released subspace specimens."));
        g->sound(g->u.posx, g->u.posy, 40, _("An alarm sounds!"));
        g->m.translate_radius(t_reinforced_glass_h, t_floor, 25.0, g->u.posx, g->u.posy);
        g->m.translate_radius(t_reinforced_glass_v, t_floor, 25.0, g->u.posx, g->u.posy);
        query_any(_("Containment shields opened.  Press any key..."));
        break;

    case COMPACT_RELEASE_BIONICS:
        g->sound(g->u.posx, g->u.posy, 40, _("An alarm sounds!"));
        g->m.translate_radius(t_reinforced_glass_h, t_floor, 2.0, g->u.posx, g->u.posy);
        g->m.translate_radius(t_reinforced_glass_v, t_floor, 2.0, g->u.posx, g->u.posy);
        query_any(_("Containment shields opened.  Press any key..."));
        break;

    case COMPACT_TERMINATE:
        g->u.add_memorial_log(pgettext("memorial_male", "Terminated subspace specimens."),
                              pgettext("memorial_female", "Terminated subspace specimens."));
        for (int x = 0; x < SEEX * MAPSIZE; x++) {
            for (int y = 0; y < SEEY * MAPSIZE; y++) {
                int mondex = g->mon_at(x, y);
                if (mondex != -1 &&
                    ((g->m.ter(x, y - 1) == t_reinforced_glass_h &&
                      g->m.ter(x, y + 1) == t_concrete_h) ||
                     (g->m.ter(x, y + 1) == t_reinforced_glass_h &&
                      g->m.ter(x, y - 1) == t_concrete_h))) {
                    g->zombie( mondex ).die( &g->u );
                }
            }
        }
        query_any(_("Subjects terminated.  Press any key..."));
        break;

    case COMPACT_PORTAL:
        g->u.add_memorial_log(pgettext("memorial_male", "Opened a portal."),
                              pgettext("memorial_female", "Opened a portal."));
        for (int i = 0; i < SEEX * MAPSIZE; i++) {
            for (int j = 0; j < SEEY * MAPSIZE; j++) {
                int numtowers = 0;
                for (int xt = i - 2; xt <= i + 2; xt++) {
                    for (int yt = j - 2; yt <= j + 2; yt++) {
                        if (g->m.ter(xt, yt) == t_radio_tower) {
                            numtowers++;
                        }
                    }
                }
                if (numtowers == 4) {
                    if (g->m.tr_at(i, j) == tr_portal) {
                        g->m.remove_trap(i, j);
                    } else {
                        g->m.add_trap(i, j, tr_portal);
                    }
                }
            }
        }
        break;

    case COMPACT_CASCADE: {
        if (!query_bool(_("WARNING: Resonance cascade carries severe risk!  Continue?"))) {
            return;
        }
        g->u.add_memorial_log(pgettext("memorial_male", "Caused a resonance cascade."),
                              pgettext("memorial_female", "Caused a resonance cascade."));
        std::vector<point> cascade_points;
        for (int i = g->u.posx - 10; i <= g->u.posx + 10; i++) {
            for (int j = g->u.posy - 10; j <= g->u.posy + 10; j++) {
                if (g->m.ter(i, j) == t_radio_tower) {
                    cascade_points.push_back(point(i, j));
                }
            }
        }
        if (cascade_points.empty()) {
            g->resonance_cascade(g->u.posx, g->u.posy);
        } else {
            point p = cascade_points[rng(0, cascade_points.size() - 1)];
            g->resonance_cascade(p.x, p.y);
        }
    }
    break;

    case COMPACT_RESEARCH: {
        std::string log;
        if (lab_notes.empty()) {
            log = _("No data found.");
        } else {
            g->u.moves -= 70;
            log = lab_notes[(g->get_abs_levx() + g->get_abs_levy() + g->get_abs_levz() + alerts) %
                            lab_notes.size()];
        }

        print_text("%s", log.c_str());
        // One's an anomaly
        if (alerts == 0) {
            query_any(_("Local data-access error logged, alerting helpdesk. Press any key..."));
            alerts ++;
        } else {
            // Two's a trend.
            query_any(_("Warning: anomalous archive-access activity detected at this node. Press any key..."));
            alerts ++;
        }
    }
    break;

    case COMPACT_MAPS: {
        g->u.moves -= 30;
        const tripoint center = g->om_global_location();
        overmap_buffer.reveal(point(center.x, center.y), 40, 0);
        query_any(_("Surface map data downloaded.  Press any key..."));
    }
    break;

    case COMPACT_MAP_SEWER: {
        g->u.moves -= 30;
        const tripoint center = g->om_global_location();
        for (int i = -60; i <= 60; i++) {
            for (int j = -60; j <= 60; j++) {
                const oter_id &oter = overmap_buffer.ter(center.x + i, center.y + j, center.z);
                if (is_ot_type("sewer", oter) || is_ot_type("sewage", oter)) {
                    overmap_buffer.set_seen(center.x + i, center.y + j, center.z, true);
                }
            }
        }
        query_any(_("Sewage map data downloaded.  Press any key..."));
    }
    break;


    case COMPACT_MISS_LAUNCH: {
        // Target Acquisition.
        tripoint target = overmap::draw_overmap(0);
        if (target == overmap::invalid_tripoint) {
            add_msg(m_info, _("Target acquisition canceled"));
            return;
        }
        if(query_yn(_("Confirm nuclear missile launch."))) {
            add_msg(m_info, _("Nuclear missile launched!"));
            options.clear();//Remove the option to fire another missle.
        } else {
            add_msg(m_info, _("Nuclear missile launch aborted."));
            return;
        }
        g->refresh_all();

        //Put some smoke gas and explosions at the nuke location.
        for(int i = g->u.posx + 8; i < g->u.posx + 15; i++) {
            for(int j = g->u.posy + 3; j < g->u.posy + 12; j++)
                if(!one_in(4)) {
                    g->m.add_field(i + rng(-2, 2), j + rng(-2, 2), fd_smoke, rng(1, 9));
                }
        }

        g->explosion(g->u.posx + 10, g->u.posx + 21, 200, 0, true); //Only explode once. But make it large.

        //...ERASE MISSILE, OPEN SILO, DISABLE COMPUTER
        // For each level between here and the surface, remove the missile
        for (int level = g->levz; level <= 0; level++) {
            map tmpmap;
            tmpmap.load(g->levx, g->levy, level, false, g->cur_om);

            if(level < 0) {
                tmpmap.translate(t_missile, t_hole);
            } else if(level == 0) {
                tmpmap.translate(t_metal_floor, t_hole);
            }
            tmpmap.save();
        }

        const oter_id oter = overmap_buffer.ter(target.x, target.y, 0);
        //~ %s is terrain name
        g->u.add_memorial_log( pgettext("memorial_male", "Launched a nuke at a %s."),
                               pgettext("memorial_female", "Launched a nuke at a %s."),
                               otermap[oter].name.c_str() );
        for(int x = target.x - 2; x <= target.x + 2; x++) {
            for(int y = target.y - 2; y <= target.y + 2; y++) {
                // give it a nice rounded shape
                if(!(x == (target.x - 2) && (y == (target.y - 2))) &&
                   !(x == (target.x - 2) && (y == (target.y + 2))) &&
                   !(x == (target.x + 2) && (y == (target.y - 2))) &&
                   !(x == (target.x + 2) && (y == (target.y + 2)))) {
                    g->nuke(x, y);
                }

            }
        }

        activate_failure(COMPFAIL_SHUTDOWN);
    }
    break;


    case COMPACT_MISS_DISARM: // TODO: stop the nuke from creating radioactive clouds.
        if(query_yn(_("Disarm missile."))) {
            g->u.add_memorial_log(pgettext("memorial_male", "Disarmed a nuclear missile."),
                                  pgettext("memorial_female", "Disarmed a nuclear missile."));
            add_msg(m_info, _("Nuclear missile disarmed!"));
            options.clear();//disable missile.
            activate_failure(COMPFAIL_SHUTDOWN);
        } else {
            add_msg(m_neutral, _("Nuclear missile remains active."));
            return;
        }
        break;

    case COMPACT_LIST_BIONICS: {
        g->u.moves -= 30;
        std::vector<std::string> names;
        int more = 0;
        for (int x = 0; x < SEEX * MAPSIZE; x++) {
            for (int y = 0; y < SEEY * MAPSIZE; y++) {
                for (std::vector<item>::iterator it = g->m.i_at(x, y).begin();
                     it != g->m.i_at(x, y).end(); ++it) {
                    if (it->is_bionic()) {
                        if ((int)names.size() < TERMY - 8) {
                            names.push_back(it->tname());
                        } else {
                            more++;
                        }
                    }
                }
            }
        }

        reset_terminal();

        print_newline();
        print_line(_("Bionic access - Manifest:"));
        print_newline();

        for (std::vector<std::string>::iterator it = names.begin();
             it != names.end(); ++it) {
            print_line("%s", it->c_str());
        }
        if (more > 0) {
            print_line(ngettext("%d OTHER FOUND...", "%d OTHERS FOUND...", more), more);
        }

        print_newline();
        query_any(_("Press any key..."));
    }
    break;

    case COMPACT_ELEVATOR_ON:
        for (int x = 0; x < SEEX * MAPSIZE; x++) {
            for (int y = 0; y < SEEY * MAPSIZE; y++) {
                if (g->m.ter(x, y) == t_elevator_control_off) {
                    g->m.ter_set(x, y, t_elevator_control);
                }
            }
        }
        query_any(_("Elevator activated.  Press any key..."));
        break;

    case COMPACT_AMIGARA_LOG: // TODO: This is static, move to data file?
        g->u.moves -= 30;
        reset_terminal();
        print_line(_("NEPower Mine(%d:%d) Log"), g->get_abs_levx(), g->get_abs_levy());
        print_line(_("\
ENTRY 47:\n\
Our normal mining routine has unearthed a hollow chamber.  This would not be\n\
out of the ordinary, save for the odd, perfectly vertical faultline found.\n\
This faultline has several odd concavities in it which have the more\n\
superstitious crew members alarmed; they seem to be of human origin.\n\
\n\
ENTRY 48:\n\
The concavities are between 10 and 20 feet tall, and run the length of the\n\
faultline.  Each one is vaguely human in shape, but with the proportions of\n\
the limbs, neck and head greatly distended, all twisted and curled in on\n\
themselves.\n"));
        if (!query_bool(_("Continue reading?"))) {
            return;
        }
        g->u.moves -= 30;
        reset_terminal();
        print_line(_("NEPower Mine(%d:%d) Log"), g->get_abs_levx(), g->get_abs_levy());
        print_line(_("\
ENTRY 49:\n\
We've stopped mining operations in this area, obviously, until archaeologists\n\
have the chance to inspect the area.  This is going to set our schedule back\n\
by at least a week.  This stupid artifact-preservation law has been in place\n\
for 50 years, and hasn't even been up for termination despite the fact that\n\
these mining operations are the backbone of our economy.\n\
\n\
ENTRY 52:\n\
Still waiting on the archaeologists.  We've done a little light insepction of\n\
the faultline; our sounding equipment is insufficient to measure the depth of\n\
the concavities.  The equipment is rated at 15 miles depth, but it isn't made\n\
for such narrow tunnels, so it's hard to say exactly how far back they go.\n"));
        if (!query_bool(_("Continue reading?"))) {
            return;
        }
        g->u.moves -= 30;
        reset_terminal();
        print_line(_("NEPower Mine(%d:%d) Log"), g->get_abs_levx(), g->get_abs_levy());
        print_line(_("\
ENTRY 54:\n\
I noticed a couple of the guys down in the chamber with a chisel, breaking\n\
off a piece of the sheer wall.  I'm looking the other way.  It's not like\n\
the eggheads are going to notice a little piece missing.  Fuck em.\n\
\n\
ENTRY 55:\n\
Well, the archaeologists are down there now with a couple of the boys as\n\
guides.  They're hardly Indiana Jones types; I doubt they been below 20\n\
feet.  I hate taking guys off assignment just to babysit the scientists, but\n\
if they get hurt we'll be shut down for god knows how long.\n\
\n\
ENTRY 58:\n\
They're bringing in ANOTHER CREW?  Christ, it's just some cave carvings!  I\n\
know that's sort of a big deal, but come on, these guys can't handle it?\n"));
        if (!query_bool(_("Continue reading?"))) {
            return;
        }
        reset_terminal();
        for (int i = 0; i < 10; i++) {
            print_gibberish_line();
        }
        print_newline();
        print_newline();
        print_newline();
        print_line(_("AMIGARA PROJECT"));
        print_newline();
        print_newline();
        if (!query_bool(_("Continue reading?"))) {
            return;
        }
        g->u.moves -= 30;
        reset_terminal();
        print_line(_("\
SITE %d%d%d\n\
PERTINANT FOREMAN LOGS WILL BE PREPENDED TO NOTES"),
                   g->get_abs_levx(), g->get_abs_levy(), abs(g->get_abs_levz()));
        print_line(_("\n\
MINE OPERATIONS SUSPENDED; CONTROL TRANSFERRED TO AMIGARA PROJECT UNDER\n\
   IMPERATIVE 2:07B\n\
FAULTLINE SOUNDING HAS PLACED DEPTH AT 30.09 KM\n\
DAMAGE TO FAULTLINE DISCOVERED; NEPOWER MINE CREW PLACED UNDER ARREST FOR\n\
   VIOLATION OF REGULATION 87.08 AND TRANSFERRED TO LAB 89-C FOR USE AS\n\
   SUBJECTS\n\
QUALITIY OF FAULTLINE NOT COMPROMISED\n\
INITIATING STANDARD TREMOR TEST..."));
        print_gibberish_line();
        print_gibberish_line();
        print_newline();
        print_error(_("FILE CORRUPTED, PRESS ANY KEY..."));
        getch();
        reset_terminal();
        break;

    case COMPACT_AMIGARA_START:
        g->add_event(EVENT_AMIGARA, int(calendar::turn) + 10);
        if (!g->u.has_artifact_with(AEP_PSYSHIELD)) {
            g->u.add_disease("amigara", 20);
        }
        break;

    case COMPACT_STEMCELL_TREATMENT:
        g->u.moves -= 70;
        g->u.add_disease("stemcell_treatment", 120);
        print_line(_("The machine injects your eyeball with the solution \n\
of pureed bone & LSD."));
        query_any(_("Press any key..."));
        g->u.mod_pain( rng(40, 90) );
        break;

    case COMPACT_DOWNLOAD_SOFTWARE:
        if (!g->u.has_amount("usb_drive", 1)) {
            print_error(_("USB drive required!"));
        } else {
            mission *miss = g->find_mission(mission_id);
            if (miss == NULL) {
                debugmsg(_("Computer couldn't find its mission!"));
                return;
            }
            g->u.moves -= 30;
            item software(miss->item_id, 0);
            software.mission_id = mission_id;
            item *usb = g->u.pick_usb();
            usb->contents.clear();
            usb->put_in(software);
            print_line(_("Software downloaded."));
        }
        getch();
        break;

    case COMPACT_BLOOD_ANAL:
        g->u.moves -= 70;
        for (int x = g->u.posx - 2; x <= g->u.posx + 2; x++) {
            for (int y = g->u.posy - 2; y <= g->u.posy + 2; y++) {
                if (g->m.ter(x, y) == t_centrifuge) {
                    if (g->m.i_at(x, y).empty()) {
                        print_error(_("ERROR: Please place sample in centrifuge."));
                    } else if (g->m.i_at(x, y).size() > 1) {
                        print_error(_("ERROR: Please remove all but one sample from centrifuge."));
                    } else if (g->m.i_at(x, y)[0].type->id != "vacutainer") {
                        print_error(_("ERROR: Please use vacutainer-contained samples."));
                    } else if (g->m.i_at(x, y)[0].contents.empty()) {
                        print_error(_("ERROR: Vacutainer empty."));
                    } else if (g->m.i_at(x, y)[0].contents[0].type->id != "blood") {
                        print_error(_("ERROR: Please only use blood samples."));
                    } else { // Success!
                        item *blood = &(g->m.i_at(x, y)[0].contents[0]);
                        if (blood->corpse == NULL || blood->corpse->id == "mon_null") {
                            print_line(_("Result:  Human blood, no pathogens found."));
                        } else if( blood->corpse->in_species( "ZOMBIE" ) ) {
                            if( blood->corpse->sym == "Z" ) {
                                print_line(_("Result:  Human blood.  Unknown pathogen found."));
                            } else {
                                print_line(_("Result:  Unknown blood type.  Unknown pathogen found."));
                            }
                            print_line(_("Pathogen bonded to erythrocytes and leukocytes."));
                            if (query_bool(_("Download data?"))) {
                                if (!g->u.has_amount("usb_drive", 1)) {
                                    print_error(_("USB drive required!"));
                                } else {
                                    item software("software_blood_data", 0);
                                    item *usb = g->u.pick_usb();
                                    usb->contents.clear();
                                    usb->put_in(software);
                                    print_line(_("Software downloaded."));
                                }
                            }
                        } else {
                            print_line(_("Result: Unknown blood type.  Test nonconclusive."));
                        }
                    }
                }
            }
        }
        query_any(_("Press any key..."));
        break;

    case COMPACT_DATA_ANAL:
        g->u.moves -= 30;
        for (int x = g->u.posx - 2; x <= g->u.posx + 2; x++) {
            for (int y = g->u.posy - 2; y <= g->u.posy + 2; y++) {
                if (g->m.ter(x, y) == t_floor_blue) {
                    print_error(_("PROCESSING DATA"));
                    if (g->m.i_at(x, y).empty()) {
                        print_error(_("ERROR: Please place memory bank in scan area."));
                    } else if (g->m.i_at(x, y).size() > 1) {
                        print_error(_("ERROR: Please only scan one item at a time."));
                    } else if (g->m.i_at(x, y)[0].type->id != "usb_drive" &&
                               g->m.i_at(x, y)[0].type->id != "black_box") {
                        print_error(_("ERROR: Memory bank destroyed or not present."));
                    } else if (g->m.i_at(x, y)[0].type->id == "usb_drive" && g->m.i_at(x, y)[0].contents.empty()) {
                        print_error(_("ERROR: Memory bank is empty."));
                    } else { // Success!
                        if (g->m.i_at(x, y)[0].type->id == "black_box") {
                            print_line(_("Memory Bank:  Military Hexron Encryption\nPrinting Transcript\n"));
                            item transcript("black_box_transcript", calendar::turn);
                            g->m.add_item_or_charges(g->u.posx, g->u.posy, transcript);
                        } else {
                            print_line(_("Memory Bank:  Unencrypted\nNothing of interest.\n"));
                        }

                    }
                }
            }
        }
        query_any(_("Press any key..."));
        break;

    case COMPACT_DISCONNECT:
        reset_terminal();
        print_line(_("\n\
ERROR:  NETWORK DISCONNECT \n\
UNABLE TO REACH NETWORK ROUTER OR PROXY.  PLEASE CONTACT YOUR\n\
SYSTEM ADMINISTRATOR TO RESOLVE THIS ISSUE.\n\
  \n"));
        query_any(_("Press any key to continue..."));
        break;

    case COMPACT_EMERG_MESS:
        print_line(_("\
GREETINGS CITIZEN. A BIOLOGICAL ATTACK HAS TAKEN PLACE AND A STATE OF \n\
EMERGENCY HAS BEEN DECLARED. EMERGENCY PERSONNEL WILL BE AIDING YOU \n\
SHORTLY. TO ENSURE YOUR SAFETY PLEASE FOLLOW THE BELOW STEPS. \n\
\n\
1. DO NOT PANIC. \n\
2. REMAIN INSIDE THE BUILDING. \n\
3. SEEK SHELTER IN THE BASEMENT. \n\
4. USE PROVIDED GAS MASKS. \n\
5. AWAIT FURTHER INSTRUCTIONS \n\
\n\
  \n"));
        query_any(_("Press any key to continue..."));
        break;

    case COMPACT_TOWER_UNRESPONSIVE:
        print_line(_("\
  WARNING, RADIO TOWER IS UNRESPONSIVE. \n\
  \n\
  BACKUP POWER INSUFFICIENT TO MEET BROADCASTING REQUIREMENTS. \n\
  IN THE EVENT OF AN EMERGENCY, CONTACT LOCAL NATIONAL GUARD \n\
  UNITS TO RECEIVE PRIORITY WHEN GENERATORS ARE BEING DEPLOYED. \n\
  \n\
  \n"));
        query_any(_("Press any key to continue..."));
        break;

    case COMPACT_SR1_MESS:
        reset_terminal();
        print_line(_("\n\
  Subj: Security Reminder\n\
  To: all SRCF staff\n\
  From: Constantine Dvorak, Undersecretary of Nuclear Security\n\
  \n\
      I want to remind everyone on staff: Do not open or examine\n\
  containers above your security-clearance.  If you have some\n\
  question about safety protocols or shipping procedures, please\n\
  contact your SRCF administrator or on-site military officer.\n\
  When in doubt, assume all containers are Class-A Biohazards\n\
  and highly toxic. Take full precautions!\n\
  \n"));
        query_any(_("Press any key to continue..."));
        break;

    case COMPACT_SR2_MESS:
        reset_terminal();
        print_line(_("\n\
  Subj: Security Reminder\n\
  To: all SRCF staff\n\
  From: Constantine Dvorak, Undersecretary of Nuclear Security\n\
  \n\
  From today onward medical wastes are not to be stored anywhere\n\
  near radioactive materials.  All containers are to be\n\
  re-arranged according to these new regulations.  If your\n\
  facility currently has these containers stored in close\n\
  proximity, you are to work with armed guards on duty at all\n\
  times. Report any unusual activity to your SRCF administrator\n\
  at once.\n\
  "));
        query_any(_("Press any key to continue..."));
        break;

    case COMPACT_SR3_MESS:
        reset_terminal();
        print_line(_("\n\
  Subj: Security Reminder\n\
  To: all SRCF staff\n\
  From: Constantine Dvorak, Undersecretary of Nuclear Security\n\
  \n\
  Worker health and safety is our number one concern!  As such,\n\
  we are instituting weekly health examinations for all SRCF\n\
  employees.  Report any unusual symptoms or physical changes\n\
  to your SRCF administrator at once.\n\
  "));
        query_any(_("Press any key to continue..."));
        break;

    case COMPACT_SR4_MESS:
        reset_terminal();
        print_line(_("\n\
  Subj: Security Reminder\n\
  To: all SRCF staff\n\
  From:  Constantine Dvorak, Undersecretary of Nuclear Security\n\
  \n\
  All compromised facilities will remain under lock down until\n\
  further notice.  Anyone who has seen or come in direct contact\n\
  with the creatures is to report to the home office for a full\n\
  medical evaluation and security debriefing.\n\
  "));
        query_any(_("Press any key to continue..."));
        break;

    case COMPACT_SRCF_1_MESS:
        reset_terminal();
        print_line(_("\n\
  Subj: EPA: Report All Potential Containment Breaches 3873643\n\
  To: all SRCF staff\n\
  From:  Robert Shane, Director of the EPA\n\
  \n\
  All hazardous waste dumps and sarcophagi must submit three\n\
  samples from each operational leache system to the following\n\
  addresses:\n\
  \n\
  CDC Bioterrorism Lab \n\
  Building 10\n\
  Corporate Square Boulevard\n\
  Atlanta, GA 30329\n\
  \n\
  EPA Region 8 Laboratory\n\
  16194 W. 45th\n\
  Drive Golden, Colorado 80403\n\
  \n\
  These samples must be accurate and any attempts to cover\n\
  incompetencies will result in charges of Federal Corruption\n\
  and potentially Treason.\n"));
        query_any(_("Press any key to continue..."));
        reset_terminal();
        print_line(_("Director of the EPA,\n\
  Robert Shane\n\
  \n"));
        query_any(_("Press any key to continue..."));
        break;

    case COMPACT_SRCF_2_MESS:
        reset_terminal();
        print_line(_(" Subj: SRCF: Internal Memo, EPA [2918024]\n\
  To: all SRCF admin staff\n\
  From:  Constantine Dvorak, Undersecretary of Nuclear Security\n\
  \n\
  Director Grimes has released a new series of accusations that\n\
  will soon be investigated by a Congressional committee.  Below\n\
  is the message that he sent me.\n\
  \n\
  --------------------------------------------------------------\n\
  Subj: Congressional Investigations\n\
  To: Constantine Dvorak, Undersecretary of Nuclear Safety\n\
  From: Robert Shane, director of the EPA\n\
  \n\
      The EPA has opposed the Security-Restricted Containment\n\
  Facility (SRCF) project from its inception.  We were horrified\n\
  that these facilities would be constructed so close to populated\n\
  areas, and only agreed to sign-off on the project if we were\n\
  allowed to freely examine and monitor the sarcophagi.  But that\n\
  has not happened.  Since then, the DoE has employed any and all\n\
  means to keep EPA agents from visiting the SRCFs, using military\n\
  secrecy, emergency powers, and inter-departmental gag orders to\n"));
        query_any(_("Press any key to continue..."));
        reset_terminal();
        print_line(_(" surround the project with an impenetrable thicket of red tape.\n\
  \n\
      Although our agents have not been allowed inside, our atmospheric\n\
  testers in nearby communities have detected high levels of toxins\n\
  and radiation, and we've found dozens of potentially dangerous\n\
  unidentified compounds in the ground water.  We now have\n\
  conclusive evidence that the SRCFs are a threat to the public\n\
  safety.  We are taking these data to state representatives and\n\
  petitioning for a full congressional inquiry.  They should be\n\
  able to force open your secret vaults, and the world will see\n\
  what you've been hiding.\n\
  \n\
  If you had any hand in this outbreak I hope you rot in hell.\n\
  \n\
  Director of the EPA,\n\
  Robert Shane\n\
  \n"));
        query_any(_("Press any key to continue..."));
        break;

    case COMPACT_SRCF_3_MESS:
        reset_terminal();
        print_line(_(" Subj: CDC: Internal Memo, Standby [2918115]\n\
  To: all SRCF staff\n\
  From:  Ellen Grimes, Director of the EPA\n\
  \n\
      Your site along with many others has been found to be\n\
  contaminated with what we will now refer to as [redacted].\n\
  It is vital that you standby for further orders.  We are\n\
  currently awaiting the President to decide our course of\n\
  action in this national crisis.  You will proceed with fail-\n\
  safe procedures and rig the sarcophagus with c-4 as outlined\n\
  in Publication 4423.  We will send you orders to either detonate\n\
  and seal the sarcophagus or remove the charges.  It is of the\n\
  utmost importance that the facility be sealed immediatly when\n\
  the orders are given.  We have been alerted by Homeland Security\n\
  that there are potential terrorist suspects that are being\n\
  detained in connection with the recent national crisis.\n\
  \n\
  Director of the CDC,\n\
  Ellen Grimes\n\
  \n"));
        query_any(_("Press any key to continue..."));
        break;

    case COMPACT_SRCF_SEAL_ORDER:
        reset_terminal();
        print_line(_(" Subj: USARMY: SEAL SRCF [987167]\n\
  To: all SRCF staff\n\
  From:  Major General Cornelius, U.S. Army\n\
  \n\
    As a general warning to all civilian staff: the 10th Mountain\n\
  Division has been assigned to oversee the sealing of the SRCF\n\
  facilities.  By direct order, all non-essential staff must vacate\n\
  at the earliest possible opportunity to prevent potential\n\
  contamination.  Low yield tactical nuclear demolition charges\n\
  will be deployed in the lower tunnels to ensure that recovery\n\
  of hazardous material is impossible.  The Army Corps of Engineers\n\
  will then dump concrete over the rubble so that we can redeploy \n\
  the 10th Mountain into the greater Boston area.\n\
  \n\
  Cornelius,\n\
  Major General, U.S. Army\n\
  Commander of the 10th Mountain Division\n\
  \n"));
        query_any(_("Press any key to continue..."));
        break;

    case COMPACT_SRCF_SEAL:
        g->u.add_memorial_log(pgettext("memorial_male", "Sealed a Hazardous Material Sarcophagus."),
                              pgettext("memorial_female", "Sealed a Hazardous Material Sarcophagus."));
        add_msg(m_warning, _("Evacuate Immediately!"));
        for (int x = 0; x < SEEX * MAPSIZE; x++) {
            for (int y = 0; y < SEEY * MAPSIZE; y++) {
                if (g->m.ter(x, y) == t_elevator || g->m.ter(x, y) == t_vat) {
                    g->m.make_rubble(x, y, f_rubble_rock, true);
                    g->explosion(x, y, 40, 0, true);
                }
                if (g->m.ter(x, y) == t_wall_glass_h || g->m.ter(x, y) == t_wall_glass_v) {
                    g->m.make_rubble(x, y, f_rubble_rock, true);
                }
                if (g->m.ter(x, y) == t_sewage_pipe || g->m.ter(x, y) == t_sewage || g->m.ter(x, y) == t_grate) {
                    g->m.make_rubble(x, y, f_rubble_rock, true);
                }
                if (g->m.ter(x, y) == t_sewage_pump) {
                    g->m.make_rubble(x, y, f_rubble_rock, true);
                    g->explosion(x, y, 50, 0, true);
                }
            }
        }
        break;

    case COMPACT_SRCF_ELEVATOR:
        if (!g->u.has_amount("sarcophagus_access_code", 1)) {
            print_error(_("Access code required!"));
        } else {
            g->u.use_amount("sarcophagus_access_code", 1);
            reset_terminal();
            print_line(
                _("\nPower:         Backup Only\nRadion Level:  Very Dangerous\nOperational:   Overrided\n\n"));
            for (int x = 0; x < SEEX * MAPSIZE; x++) {
                for (int y = 0; y < SEEY * MAPSIZE; y++) {
                    if (g->m.ter(x, y) == t_elevator_control_off) {
                        g->m.ter_set(x, y, t_elevator_control);

                    }
                }
            }
        }
        query_any(_("Press any key..."));
        break;

    } // switch (action)
}

void computer::activate_random_failure()
{
    computer_failure fail = (failures.empty() ? COMPFAIL_SHUTDOWN :
                             failures[rng(0, failures.size() - 1)]);
    activate_failure(fail);
}

void computer::activate_failure(computer_failure fail)
{
    bool found_tile = false;
    switch (fail) {

    case COMPFAIL_NULL: // Unknown action.
    case NUM_COMPUTER_FAILURES: // Suppress compiler warning [-Wswitch]
        break;

    case COMPFAIL_SHUTDOWN:
        for( int x = g->u.posx - 1; x <= g->u.posx + 1; x++ ) {
            for( int y = g->u.posy - 1; y <= g->u.posy + 1; y++ ) {
                if( g->m.has_flag("CONSOLE", x, y) ) {
                    g->m.ter_set(x, y, t_console_broken);
                    add_msg(m_bad, _("The console shuts down."));
                    found_tile = true;
                }
            }
        }
        if( found_tile ) {
            break;
        }
        for (int x = 0; x < SEEX * MAPSIZE; x++) {
            for (int y = 0; y < SEEY * MAPSIZE; y++) {
                if (g->m.has_flag("CONSOLE", x, y)) {
                    g->m.ter_set(x, y, t_console_broken);
                    add_msg(m_bad, _("The console shuts down."));
                }
            }
        }
        break;

    case COMPFAIL_ALARM:
        g->u.add_memorial_log(pgettext("memorial_male", "Set off an alarm."),
                              pgettext("memorial_female", "Set off an alarm."));
        g->sound(g->u.posx, g->u.posy, 60, _("An alarm sounds!"));
        if (g->levz > 0 && !g->event_queued(EVENT_WANTED)) {
            g->add_event(EVENT_WANTED, int(calendar::turn) + 300, 0, g->get_abs_levx(), g->get_abs_levy());
        }
        break;

    case COMPFAIL_MANHACKS: {
        int num_robots = rng(4, 8);
        for (int i = 0; i < num_robots; i++) {
            int mx, my, tries = 0;
            do {
                mx = rng(g->u.posx - 3, g->u.posx + 3);
                my = rng(g->u.posy - 3, g->u.posy + 3);
                tries++;
            } while (!g->is_empty(mx, my) && tries < 10);
            if (tries != 10) {
                add_msg(m_warning, _("Manhacks drop from compartments in the ceiling."));
                monster robot(GetMType("mon_manhack"));
                robot.spawn(mx, my);
                g->add_zombie(robot);
            }
        }
    }
    break;

    case COMPFAIL_SECUBOTS: {
        int num_robots = 1;
        for (int i = 0; i < num_robots; i++) {
            int mx, my, tries = 0;
            do {
                mx = rng(g->u.posx - 3, g->u.posx + 3);
                my = rng(g->u.posy - 3, g->u.posy + 3);
                tries++;
            } while (!g->is_empty(mx, my) && tries < 10);
            if (tries != 10) {
                add_msg(m_warning, _("Secubots emerge from compartments in the floor."));
                monster robot(GetMType("mon_secubot"));
                robot.spawn(mx, my);
                g->add_zombie(robot);
            }
        }
    }
    break;

    case COMPFAIL_DAMAGE:
        add_msg(m_neutral, _("The console electrocutes you."));
        if (g->u.has_artifact_with(AEP_RESIST_ELECTRICITY) ||
            g->u.has_active_bionic("bio_faraday")) { //Artifact or bionic stops electricity.
            add_msg(m_neutral, _("The electricity flows around you."));
        } else if (g->u.worn_with_flag("ELECTRIC_IMMUNE")) { //Armor stops electricity.
            add_msg(m_neutral, _("Your armor safely grounds the electrical discharge."));
        } else {
            add_msg(m_bad, _("Your body is damaged by the electric shock!"));
            g->u.hurtall(rng(1, 10));
        }
        break;

    case COMPFAIL_PUMP_EXPLODE:
        add_msg(m_warning, _("The pump explodes!"));
        for (int x = 0; x < SEEX * MAPSIZE; x++) {
            for (int y = 0; y < SEEY * MAPSIZE; y++) {
                if (g->m.ter(x, y) == t_sewage_pump) {
                    g->m.make_rubble(x, y);
                    g->explosion(x, y, 10, 0, false);
                }
            }
        }
        break;

    case COMPFAIL_PUMP_LEAK:
        add_msg(m_warning, _("Sewage leaks!"));
        for (int x = 0; x < SEEX * MAPSIZE; x++) {
            for (int y = 0; y < SEEY * MAPSIZE; y++) {
                if (g->m.ter(x, y) == t_sewage_pump) {
                    point p(x, y);
                    int leak_size = rng(4, 10);
                    for (int i = 0; i < leak_size; i++) {
                        std::vector<point> next_move;
                        if (g->m.move_cost(p.x, p.y - 1) > 0) {
                            next_move.push_back( point(p.x, p.y - 1) );
                        }
                        if (g->m.move_cost(p.x + 1, p.y) > 0) {
                            next_move.push_back( point(p.x + 1, p.y) );
                        }
                        if (g->m.move_cost(p.x, p.y + 1) > 0) {
                            next_move.push_back( point(p.x, p.y + 1) );
                        }
                        if (g->m.move_cost(p.x - 1, p.y) > 0) {
                            next_move.push_back( point(p.x - 1, p.y) );
                        }

                        if (next_move.empty()) {
                            i = leak_size;
                        } else {
                            p = next_move[rng(0, next_move.size() - 1)];
                            g->m.ter_set(p.x, p.y, t_sewage);
                        }
                    }
                }
            }
        }
        break;

    case COMPFAIL_AMIGARA:
        g->add_event(EVENT_AMIGARA, int(calendar::turn) + 5);
        g->u.add_disease("amigara", 20);
        g->explosion(rng(0, SEEX * MAPSIZE), rng(0, SEEY * MAPSIZE), 10, 10, false);
        g->explosion(rng(0, SEEX * MAPSIZE), rng(0, SEEY * MAPSIZE), 10, 10, false);
        break;

    case COMPFAIL_DESTROY_BLOOD:
        print_error(_("ERROR: Disruptive Spin"));
        for (int x = g->u.posx - 2; x <= g->u.posx + 2; x++) {
            for (int y = g->u.posy - 2; y <= g->u.posy + 2; y++) {
                if (g->m.ter(x, y) == t_centrifuge) {
                    if (g->m.i_at(x, y).empty()) {
                        print_error(_("ERROR: Please place sample in centrifuge."));
                    } else if (g->m.i_at(x, y).size() > 1) {
                        print_error(_("ERROR: Please remove all but one sample from centrifuge."));
                    } else if (g->m.i_at(x, y)[0].type->id != "vacutainer") {
                        print_error(_("ERROR: Please use vacutainer-contained samples."));
                    } else if (g->m.i_at(x, y)[0].contents.empty()) {
                        print_error(_("ERROR: Vacutainer empty."));
                    } else if (g->m.i_at(x, y)[0].contents[0].type->id != "blood") {
                        print_error(_("ERROR: Please only use blood samples."));
                    } else {
                        print_error(_("ERROR: Blood sample destroyed."));
                        for (std::vector<item>::iterator it = g->m.i_at(x, y).begin();
                             it != g->m.i_at(x, y).end(); ++it) {
                            it->contents.clear();
                        }
                    }
                }
            }
        }
        getch();
        break;

    case COMPFAIL_DESTROY_DATA:
        print_error(_("ERROR: ACCESSING DATA MALFUNCTION"));
        for (int x = 0; x <= 23; x++) {
            for (int y = 0; y <= 23; y++) {
                if (g->m.ter(x, y) == t_floor_blue) {
                    if (g->m.i_at(x, y).empty()) {
                        print_error(_("ERROR: Please place memory bank in scan area."));
                    } else if (g->m.i_at(x, y).size() > 1) {
                        print_error(_("ERROR: Please only scan one item at a time."));
                    } else if (g->m.i_at(x, y)[0].type->id != "usb_drive") {
                        print_error(_("ERROR: Memory bank destroyed or not present."));
                    } else if (g->m.i_at(x, y)[0].contents.empty()) {
                        print_error(_("ERROR: Memory bank is empty."));
                    } else {
                        print_error(_("ERROR: Data bank destroyed."));
                        for (std::vector<item>::iterator it = g->m.i_at(x, y).begin();
                             it != g->m.i_at(x, y).end(); ++it) {
                            it->contents.clear();
                        }
                    }
                }
            }
        }
        getch();
        break;

    }// switch (fail)
}

bool computer::query_bool(const char *mes, ...)
{
    va_list ap;
    va_start(ap, mes);
    const std::string text = vstring_format(mes, ap);
    va_end(ap);
    print_line("%s (Y/N/Q)", text.c_str());
    char ret;
    do {
        ret = getch();
    } while (ret != 'y' && ret != 'Y' && ret != 'n' && ret != 'N' && ret != 'q' &&
             ret != 'Q');
    return (ret == 'y' || ret == 'Y');
}

bool computer::query_any(const char *mes, ...)
{
    va_list ap;
    va_start(ap, mes);
    const std::string text = vstring_format(mes, ap);
    va_end(ap);
    print_line("%s", text.c_str());
    getch();
    return true;
}

char computer::query_ynq(const char *mes, ...)
{
    va_list ap;
    va_start(ap, mes);
    const std::string text = vstring_format(mes, ap);
    va_end(ap);
    print_line("%s (Y/N/Q)", text.c_str());
    char ret;
    do {
        ret = getch();
    } while (ret != 'y' && ret != 'Y' && ret != 'n' && ret != 'N' && ret != 'q' &&
             ret != 'Q');
    return ret;
}

void computer::print_line(const char *mes, ...)
{
    va_list ap;
    va_start(ap, mes);
    const std::string text = vstring_format(mes, ap);
    va_end(ap);
    wprintz(w_terminal, c_green, "%s", text.c_str());
    print_newline();
    wrefresh(w_terminal);
}

void computer::print_error(const char *mes, ...)
{
    va_list ap;
    va_start(ap, mes);
    const std::string text = vstring_format(mes, ap);
    va_end(ap);
    wprintz(w_terminal, c_red, "%s", text.c_str());
    print_newline();
    wrefresh(w_terminal);
}

void computer::print_text(const char *mes, ...)
{
    va_list ap;
    va_start(ap, mes);
    const std::string text = vstring_format(mes, ap);
    va_end(ap);
    int y = getcury(w_terminal);
    int w = getmaxx(w_terminal) - 2;
    fold_and_print(w_terminal, y, 1, w, c_green, text);
    print_newline();
    print_newline();
    wrefresh(w_terminal);
}

void computer::print_gibberish_line()
{
    std::string gibberish;
    int length = rng(50, 70);
    for (int i = 0; i < length; i++) {
        switch (rng(0, 4)) {
        case 0:
            gibberish += '0' + rng(0, 9);
            break;
        case 1:
        case 2:
            gibberish += 'a' + rng(0, 25);
            break;
        case 3:
        case 4:
            gibberish += 'A' + rng(0, 25);
            break;
        }
    }
    wprintz(w_terminal, c_yellow, "%s", gibberish.c_str());
    print_newline();
    wrefresh(w_terminal);
}

void computer::reset_terminal()
{
    werase(w_terminal);
    wmove(w_terminal, 0, 0);
    wrefresh(w_terminal);
}

void computer::print_newline()
{
    wprintz(w_terminal, c_green, "\n");
}

void computer::load_lab_note(JsonObject &jsobj)
{
    lab_notes.push_back(_(jsobj.get_string("text").c_str()));
}

void computer::clear_lab_notes()
{
    lab_notes.clear();
}
