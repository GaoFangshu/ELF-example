/**
* Copyright (c) 2017-present, Facebook, Inc.
* All rights reserved.
*
* This source code is licensed under the BSD-style license found in the
* LICENSE file in the root directory of this source tree. An additional grant
* of patent rights can be found in the PATENTS file in the same directory.
*/

#include "raw2cmd.h"
#include "../engine/cmd_specific.gen.h"

/////////// RawToCmd /////////////////
//
CmdBPtr move_event(const Unit &u, char /*hotkey*/, const PointF& p, const UnitId &target_id, const GameEnv&) {
    // Don't need to check hotkey since there is only one type of action.
    // cout << "In move command [" << hotkey << "] @" << p << " target: " << target_id << endl;
    if (target_id == INVALID && ! p.IsInvalid()) {
        // Move to a space.
        return CmdBPtr(new CmdMove(u.GetId(), p));
    } else {
        // [TODO] Following (Not implemented yet)
        return CmdBPtr();
    }
}

CmdBPtr attack_event(const Unit &u, char /*hotkey*/, const PointF&, const UnitId &target_id, const GameEnv&) {
    // Don't need to check hotkey since there is only one type of action.
    // cout << "In attack command [" << hotkey << "] @" << p << " target: " << target_id << endl;
    if (target_id != INVALID) {
        // Attack a particular unit.
        if (Player::ExtractPlayerId(target_id) != Player::ExtractPlayerId(u.GetId())) {
            // Forbid friendly fire.
            return CmdBPtr(new CmdAttack(u.GetId(), target_id));
        }
    } else {
        // [TODO] Attack on an empty location.
    }
    return CmdBPtr();
}

CmdBPtr gather_event(const Unit &u, char /*hotkey*/, const PointF&, const UnitId &target_id, const GameEnv& env) {
    // Don't need to check hotkey since there is only one type of action.
    if (target_id == INVALID) return CmdBPtr();
    // cout << "In gather command [" << hotkey << "] @" << p << " target: " << target_id << endl;

    const Unit *target = env.GetUnit(target_id);
    if (target == nullptr) return CmdBPtr();

    // Only gather from resource.
    if (target->GetUnitType() != RESOURCE) return CmdBPtr();

    UnitId base = env.FindClosestBase(u.GetPlayerId());
    if (base != INVALID) {
        return CmdBPtr(new CmdGather(u.GetId(), base, target_id));
    }
    return CmdBPtr();
}

CmdBPtr build_event(const Unit &u, char hotkey, const PointF& p, const UnitId& /*target_id*/, const GameEnv&) {
    // Send the build command.
    // cout << "In build command [" << hotkey << "] @" << p << " target: " << target_id << endl;
    UnitType t = u.GetUnitType();

    UnitType build_type;
    PointF build_p;

    // For workers: c : base, b: barracks (for workers)
    // For base: s : worker,
    // For building: m : melee attacker, r: range attacker
    // [TODO]: Make it more flexible and print corresponding prompts in GUI.
    switch(t) {
        case WORKER:
            if (p.IsInvalid()) return CmdBPtr();
            // Set the location.
            build_p = p;
            if (hotkey == 'c') build_type = BASE;
            else if (hotkey == 'b') build_type = BARRACKS;
            else return CmdBPtr();
            break;
        case BASE:
            build_p.SetInvalid();
            if (hotkey == 's') build_type = WORKER;
            else return CmdBPtr();
            break;
        case BARRACKS:
            build_p.SetInvalid();
            if (hotkey == 'm') build_type = MELEE_ATTACKER;
            else if (hotkey == 'r') build_type = RANGE_ATTACKER;
            else return CmdBPtr();
            break;
        default:
            return CmdBPtr();
    }

    return CmdBPtr(new CmdBuild(u.GetId(), build_type, build_p));
}

void RawToCmd::add_hotkey(const string& s, EventResp f) {
    for (size_t i = 0; i < s.size(); ++i) {
        _hotkey_maps.insert(make_pair(s[i], f));
    }
}

void RawToCmd::setup_hotkeys() {
    add_hotkey("a", attack_event);
    add_hotkey("~", move_event);
    add_hotkey("t", gather_event);
    add_hotkey("cbsmr", build_event);
}

RawMsgStatus RawToCmd::Process(const GameEnv &env, const string&s, CmdReceiver *receiver) {
    // Raw command:
    //   t 'L' i j: left click at (i, j)
    //   t 'R' i j: right clock at (i, j)
    //   t 'B' x0 y0 x1 y1: bounding box at (x0, y0, x1, y1)
    //   t 'S' percent: slide bar to percentage
    //   t 'F'        : faster
    //   t 'W'        : slower
    //   t 'C'        : cycle through players.
    //   t lowercase : keyboard click.
    // t is tick.
    if (s.empty()) return PROCESSED;

    Tick t;
    char c;
    float percent;
    PointF p, p2;
    set<UnitId> selected;

    cout << "Cmd: " << s << endl;

    Tick tick = receiver->GetTick();
    const RTSMap& m = env.GetMap();

    istringstream ii(s);
    ii >> t >> c;
    switch(c) {
        case 'L':
        case 'R':
            ii >> p;
            if (! m.IsIn(p)) return FAILED;
            {
            UnitId closest_id = m.GetClosestUnitId(p, 1.5);
            if (closest_id != INVALID) selected.insert(closest_id);
            }
            break;
        case 'B':
            ii >> p >> p2;
            if (! m.IsIn(p) || ! m.IsIn(p2)) return FAILED;
            // Reorder the four corners.
            if (p.x > p2.x) swap(p.x, p2.x);
            if (p.y > p2.y) swap(p.y, p2.y);
            selected = m.GetUnitIdInRegion(p, p2);
            break;
        case 'F':
            receiver->SendCmd(UICmd::GetUIFaster());
            return PROCESSED;
        case 'W':
            receiver->SendCmd(UICmd::GetUISlower());
            return PROCESSED;
        case 'C':
            receiver->SendCmd(UICmd::GetUICyclePlayer());
            return PROCESSED;
        case 'S':
            ii >> percent;
            // cout << "Get slider bar notification " << percent << endl;
            receiver->SendCmd(UICmd::GetUISlideBar(percent));
            return PROCESSED;
        case 'P':
            receiver->SendCmd(UICmd::GetToggleGamePause());
            return PROCESSED;
    }

    if (! is_mouse_motion(c)) _last_key = c;

    // cout << "#Hotkey " << _hotkey_maps.size() << "  player_id = " << _player_id << " _last_key = " << _last_key
    //     << " #selected = " << selected.size() << " #prev-selected: " << _sel_unit_ids.size() << endl;

    // Rules:
    //   1. we cannot control other player's units.
    //   2. we cannot have friendly fire (enforced in the callback function)
    bool cmd_success = false;

    if (! _sel_unit_ids.empty() && selected.size() <= 1) {
        UnitId id = (selected.empty() ? INVALID : *selected.begin());
        auto it_key = _hotkey_maps.find(_last_key);
        if (it_key != _hotkey_maps.end()) {
            EventResp f = it_key->second;
            for (auto it = _sel_unit_ids.begin(); it != _sel_unit_ids.end(); ++it) {
                if (Player::ExtractPlayerId(*it) != _player_id) continue;

                // Only command our unit.
                const Unit *u = env.GetUnit(*it);

                // u has been deleted (e.g., killed)
                // We won't delete it in our selection, since the selection will naturally update.
                if (u == nullptr) continue;

                CmdBPtr cmd = f(*u, _last_key, p, id, env);
                if (! cmd.get() || ! env.GetGameDef().unit(u->GetUnitType()).CmdAllowed(cmd->type())) continue;

                // Command successful.
                receiver->SendCmd(std::move(cmd));
                cmd_success = true;
            }
        }
    }

    // Command not issued. if it is a mouse event, simply reselect the unit (or deselect)
    if (! cmd_success && is_mouse_motion(c)) select_new_units(selected);
    if (cmd_success) _last_key = '~';

    if (t > tick) return EXCEED_TICK;
    return PROCESSED;
}
