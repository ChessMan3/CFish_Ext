/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2008 Tord Romstad (Glaurung author)
  Copyright (C) 2008-2015 Marco Costalba, Joona Kiiski, Tord Romstad
  Copyright (C) 2015-2016 Marco Costalba, Joona Kiiski, Gary Linscott, Tord Romstad

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <float.h>
#include <math.h>

#include "search.h"
#include "timeman.h"
#include "uci.h"

struct TimeManagement Time; // Our global time management struct

static const int OptimumTime = 0;
static const int MaxTime = 1;

int remaining(int myTime, int myInc, int moveOverhead,
              int movesToGo, int ply, int type)
{
  if (myTime <= 0)
    return 0;

  int moveNumber = (ply + 1) / 2;
  double ratio;    // Which ratio of myTime we are going to use. It is <= 1
  double sd = 8.5;

  // Usage of increment follows quadratic distribution with the maximum
  // at move 25
  double inc = myInc * max(55.0, 120.0 - 0.12 * (moveNumber - 25) * (moveNumber - 25));

  // In moves-to-go we distribute time according to a quadratic function
  // with the maximum around move 20 for 40 moves in y time case
  if (movesToGo) {
    ratio = (type == OptimumTime ? 1.0 : 6.0) / min(50, movesToGo);

    if (moveNumber <= 40)
      ratio *= 1.1 - 0.001 * (moveNumber - 20) * (moveNumber - 20);
    else
      ratio *= 1.5;
  }
  // Otherwise we increase usage of remaining time as the game goes on
  else {
    sd = 1 + 20 * moveNumber / (500.0 + moveNumber);
    ratio = (type == OptimumTime ? 0.017 : 0.07) * sd;
  }

  ratio = min(1.0, ratio * (1 + inc / (myTime * sd)));

  return ratio * max(0, myTime - moveOverhead);
}

// tm_init() is called at the beginning of the search and calculates the
// allowed thinking time out of the time control and current game ply. We
// support four different kinds of time controls, set in 'Limits'.
//
//  inc == 0 && movestogo == 0 means: x basetime  [sudden death!]
//  inc == 0 && movestogo != 0 means: x moves in y minutes
//  inc >  0 && movestogo == 0 means: x basetime + z increment
//  inc >  0 && movestogo != 0 means: x moves in y minutes + z increment

void time_init(int us, int ply)
{
  int moveOverhead    = option_value(OPT_MOVE_OVERHEAD);
  int npmsec          = option_value(OPT_NODES_TIME);

  // If we have to play in 'nodes as time' mode, then convert from time
  // to nodes, and use resulting values in time management formulas.
  // WARNING: Given npms (nodes per millisecond) must be much lower then
  // the real engine speed to avoid time losses.
  if (npmsec) {
    if (!Time.availableNodes) // Only once at game start
      Time.availableNodes = npmsec * Limits.time[us]; // Time is in msec

    // Convert from millisecs to nodes
    Limits.time[us] = (int)Time.availableNodes;
    Limits.inc[us] *= npmsec;
    Limits.npmsec = npmsec;
  }

  Time.startTime = Limits.startTime;
  Time.optimumTime = remaining(Limits.time[us], Limits.inc[us], moveOverhead, Limits.movestogo, ply, OptimumTime);
  Time.maximumTime = remaining(Limits.time[us], Limits.inc[us], moveOverhead, Limits.movestogo, ply, MaxTime);

  if (option_value(OPT_PONDER))
    Time.optimumTime += Time.optimumTime / 4;
}

