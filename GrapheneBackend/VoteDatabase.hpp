/*
 * Copyright 2015 Follow My Vote, Inc.
 * This file is part of The Follow My Vote Stake-Weighted Voting Application ("SWV").
 *
 * SWV is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * SWV is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with SWV.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef VOTEDATABASE_HPP
#define VOTEDATABASE_HPP

#include "Contest.hpp"
#include "Decision.hpp"

#include <graphene/chain/database.hpp>

#include <kj/debug.h>

namespace swv {

/**
 * @brief The VoteDatabase class monitors the blockchain and maintains a database of all voting-related content
 */
class VoteDatabase
{
    gch::database& chain;
    gdb::primary_index<ContestIndex>* _contestIndex = nullptr;
    gdb::primary_index<DecisionIndex>* _decisionIndex = nullptr;
public:
    VoteDatabase(gch::database& chain);

    void registerIndexes();

    gch::database& db() {
        return chain;
    }
    const gch::database& db() const {
        return chain;
    }
    auto& decisionIndex() {
        KJ_ASSERT(_decisionIndex != nullptr, "Not yet initialized: call registerIndexes first");
        return *_decisionIndex;
    }
    auto& decisionIndex() const {
        KJ_ASSERT(_decisionIndex != nullptr, "Not yet initialized: call registerIndexes first");
        return *_decisionIndex;
    }
    auto& contestIndex() {
        KJ_ASSERT(_contestIndex != nullptr, "Not yet initialized: call registerIndexes first");
        return *_contestIndex;
    }
    auto& contestIndex() const {
        KJ_ASSERT(_contestIndex != nullptr, "Not yet initialized: call registerIndexes first");
        return *_contestIndex;
    }
};

} // namespace swv

#endif // VOTEDATABASE_HPP