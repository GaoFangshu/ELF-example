/**
* Copyright (c) 2017-present, Facebook, Inc.
* All rights reserved.
*
* This source code is licensed under the BSD-style license found in the
* LICENSE file in the root directory of this source tree. An additional grant
* of patent rights can be found in the PATENTS file in the same directory.
*/

#include "game.h"

template <typename WrapperCB, typename AIComm> 
void thread_main(int game_idx, const PythonOptions &options, const std::atomic_bool &done, AIComm *ai_comm) {
    const string& replay_prefix = options.save_replay_prefix;

    // Create a game.
    RTSGameOptions op;
    op.seed = (options.seed == 0 ? 0 : options.seed + game_idx);
    op.main_loop_quota = 0;
    op.max_tick = options.max_tick;
    op.save_replay_prefix = (replay_prefix.empty() ? "" : replay_prefix + std::to_string(game_idx) + "-");
    op.snapshot_prefix = "";
    op.output_file = options.output_filename;
    op.cmd_dumper_prefix = options.cmd_dumper_prefix;

    WrapperCB wrapper(game_idx, options, ai_comm);
    wrapper.OnGameOptions(&op);

    // Note that all the bots created here will be owned by game.
    // Note that AddBot() will set its receiver. So there is no need to specify it here.
    RTSGame game(op);
    wrapper.OnGameInit(&game);

    unsigned long int seed = (op.seed == 0 ? time(NULL) : op.seed);
    std::mt19937 rng;
    rng.seed(seed);

    int iter = 0;
    while (! done) {
        wrapper.OnEpisodeStart(iter, &rng, &game);
        game.MainLoop(&done);
        game.Reset();
        ++ iter;
    }
}

inline void init_enums() {
    _init_Terrain();
    _init_UnitType();
    _init_UnitAttr();
    _init_BulletState();
    _init_Level();
    _init_AIState();
    _init_PlayerPrivilege();
    _init_CDType();
}

