/***************************************************************
 ** Song timeline for the extended Stochas fork.
 **
 ** Licensed under the GNU General Public License v3 (see COPYING).
 ** Provided "AS IS" without warranty.
 **
 ** Pure C++ with no JUCE dependency so it can be unit tested on its own.
 ** Real-time safe: no allocation, no locks.
 **
 ** A song is a list of blocks. Each block says: play scene S, R passes,
 ** where one pass lasts N steps (0 = the scene's own full loop length).
 ** Given the host's play position (in quarter-note beats) it works out
 ** which block, and which pass of that block, is playing.
 **
 ** Every scene can have its own step timing (layers in Stochas can have
 ** different steps-per-measure and speed), so lengths are worked out in
 ** beats, per scene.
 ***************************************************************/
#ifndef SCENETIMELINE_H_
#define SCENETIMELINE_H_

#include <array>

#define SCENE_MAX_BLOCKS 128
#define SCENE_MAX_SCENES 32
#define SCENE_MAX_REPEATS 999
#define SCENE_MAX_STEPS 4096

// what to do once the playhead passes the end of the last block
enum class SceneEndMode {
   Stop = 0,   // no scene is active (silence)
   Loop = 1,   // go back to the first block
   HoldLast = 2 // keep playing the last block's scene
};

struct SceneBlock {
   int scene = 0;    // 0-based scene index
   int repeats = 1;  // how many passes (>= 1)
   int steps = 0;    // length of one pass in steps. 0 = automatic (the scene's own length)
};

// How a scene counts its steps. "stepsPerBar" is how many steps fit in one bar
// (steps-per-measure multiplied by the speed factor), "loopSteps" is how many
// steps the scene plays before it repeats.
struct SceneTiming {
   double stepsPerBar = 16.0;
   int loopSteps = 16;
};

// the three numbers a Stochas layer contributes to its timing
struct LayerTimingIn {
   int numSteps = 16;
   int stepsPerMeasure = 16;
   int clockDiv = 16;   // numerator; the denominator is clockDenom
};

// A scene can have up to four layers with different timings. For the song we need one
// "reference" timing: we use the layer whose loop lasts longest (so a full pass covers
// every layer), preferring the lowest layer number on a tie.
inline SceneTiming pickSceneTiming(const LayerTimingIn *layers, int count, int clockDenom = 16) {
   SceneTiming best;
   double bestBars = -1.0;
   for (int i = 0; i < count; i++) {
      const LayerTimingIn &l = layers[i];
      if (l.numSteps < 1 || l.stepsPerMeasure < 1 || l.clockDiv < 1 || clockDenom < 1) continue;
      const double spb = (double)l.stepsPerMeasure * (double)l.clockDiv / (double)clockDenom;
      const double bars = (double)l.numSteps / spb;
      if (bars > bestBars + 1e-9) {
         bestBars = bars;
         best.stepsPerBar = spb;
         best.loopSteps = l.numSteps;
      }
   }
   return best;   // defaults (16 / 16) if no layer was usable
}

// result of asking "what's playing at this position?"
struct ScenePosition {
   bool active = false;        // false = no scene should play
   int blockIndex = -1;
   int scene = -1;
   int repeatIndex = 0;        // 0-based pass within the block
   int passSteps = 0;          // length of one pass of this block in steps
   double passBeats = 0;       // length of one pass in beats
   double blockStartBeat = 0;  // where the block began (beats from song start, within one run)
   double passStartBeat = 0;   // where the current pass began (same reference)
   double beatsIntoPass = 0;   // how far into the current pass (always >= 0)
};

class SceneTimeline {
public:
   SceneTimeline() {}

   void clear() { mCount = 0; }
   int getCount() const { return mCount; }
   SceneEndMode getEndMode() const { return mEndMode; }
   void setEndMode(SceneEndMode m) { mEndMode = m; }
   const SceneBlock &getBlock(int idx) const { return mBlocks[idx]; }

   // How scene 'scene' counts its steps. Fed in by the plugin from the scene's data.
   void setSceneTiming(int scene, SceneTiming t) {
      if (scene < 0 || scene >= SCENE_MAX_SCENES) return;
      if (!(t.stepsPerBar > 0.0)) t.stepsPerBar = 16.0;
      if (t.loopSteps < 1) t.loopSteps = 1;
      mTiming[scene] = t;
   }
   SceneTiming getSceneTiming(int scene) const {
      if (scene < 0 || scene >= SCENE_MAX_SCENES) return SceneTiming();
      return mTiming[scene];
   }

   static bool valid(int scene, int repeats, int steps) {
      return scene >= 0 && scene < SCENE_MAX_SCENES &&
             repeats >= 1 && repeats <= SCENE_MAX_REPEATS &&
             steps >= 0 && steps <= SCENE_MAX_STEPS;
   }

   // add a block at the end. Returns false (and changes nothing) if full or invalid.
   bool addBlock(int scene, int repeats = 1, int steps = 0) {
      if (mCount >= SCENE_MAX_BLOCKS || !valid(scene, repeats, steps)) return false;
      mBlocks[mCount++] = SceneBlock{scene, repeats, steps};
      return true;
   }

   // change an existing block. Returns false (and changes nothing) if invalid.
   bool setBlock(int idx, int scene, int repeats, int steps) {
      if (idx < 0 || idx >= mCount || !valid(scene, repeats, steps)) return false;
      mBlocks[idx] = SceneBlock{scene, repeats, steps};
      return true;
   }

   bool removeBlock(int idx) {
      if (idx < 0 || idx >= mCount) return false;
      for (int i = idx; i < mCount - 1; i++) mBlocks[i] = mBlocks[i + 1];
      mCount--;
      return true;
   }

   // move a block to a new position (used for reordering)
   bool moveBlock(int from, int to) {
      if (from < 0 || from >= mCount || to < 0 || to >= mCount) return false;
      SceneBlock b = mBlocks[from];
      if (from < to) for (int i = from; i < to; i++) mBlocks[i] = mBlocks[i + 1];
      else           for (int i = from; i > to; i--) mBlocks[i] = mBlocks[i - 1];
      mBlocks[to] = b;
      return true;
   }

   // length of one pass of block idx, in steps
   int passSteps(int idx) const {
      const SceneBlock &b = mBlocks[idx];
      return b.steps > 0 ? b.steps : mTiming[b.scene].loopSteps;
   }
   // length of one pass in beats (beatsPerBar is e.g. 4 for 4/4)
   double passBeats(int idx, double beatsPerBar) const {
      return (double)passSteps(idx) / mTiming[mBlocks[idx].scene].stepsPerBar * beatsPerBar;
   }
   // length of the whole block (all passes) in beats
   double blockBeats(int idx, double beatsPerBar) const {
      return passBeats(idx, beatsPerBar) * (double)mBlocks[idx].repeats;
   }
   double totalBeats(double beatsPerBar) const {
      double t = 0.0;
      for (int i = 0; i < mCount; i++) t += blockBeats(i, beatsPerBar);
      return t;
   }

   // Work out what is active at beatPosition (quarter notes from the song start).
   // beatsPerBar: 4.0 for 4/4, 3.0 for 3/4, and so on.
   // A position exactly on a block (or pass) boundary belongs to the NEXT one.
   ScenePosition query(double beatPosition, double beatsPerBar) const {
      ScenePosition r;
      if (mCount == 0 || beatsPerBar <= 0.0) return r;

      const double total = totalBeats(beatsPerBar);
      if (!(total > 0.0)) return r;
      // host positions can land a hair short of a boundary (floating point)
      double pos = beatPosition + kEpsilon;
      if (pos < 0.0) return r;     // before the song starts

      if (pos >= total) {
         switch (mEndMode) {
         case SceneEndMode::Stop:
            return r;
         case SceneEndMode::Loop: {
            double loops = (double)(long long)(pos / total);
            pos -= loops * total;
            break;
         }
         case SceneEndMode::HoldLast: {
            const int last = mCount - 1;
            const double pb = passBeats(last, beatsPerBar);
            r.active = true;
            r.blockIndex = last;
            r.scene = mBlocks[last].scene;
            r.passSteps = passSteps(last);
            r.passBeats = pb;
            r.blockStartBeat = total - blockBeats(last, beatsPerBar);
            r.repeatIndex = mBlocks[last].repeats - 1;     // stays in the final pass
            r.passStartBeat = total - pb;
            r.beatsIntoPass = pos - r.passStartBeat - kEpsilon; // keeps counting past the end
            if (r.beatsIntoPass < 0.0) r.beatsIntoPass = 0.0;
            return r;
         }
         }
      }

      double start = 0.0;
      for (int i = 0; i < mCount; i++) {
         const double len = blockBeats(i, beatsPerBar);
         if (pos < start + len) {
            const double pb = passBeats(i, beatsPerBar);
            // work the pass out from the nudged position so a boundary that lands
            // a hair short (floating point) still counts as the next pass
            int rep = (int)((pos - start) / pb);
            if (rep > mBlocks[i].repeats - 1) rep = mBlocks[i].repeats - 1;
            if (rep < 0) rep = 0;
            const double passStart = start + pb * (double)rep;
            double into = pos - passStart - kEpsilon;
            if (into < 0.0) into = 0.0;
            r.active = true;
            r.blockIndex = i;
            r.scene = mBlocks[i].scene;
            r.passSteps = passSteps(i);
            r.passBeats = pb;
            r.repeatIndex = rep;
            r.blockStartBeat = start;
            r.passStartBeat = passStart;
            r.beatsIntoPass = into;
            return r;
         }
         start += len;
      }
      return r; // should not get here
   }

private:
   static constexpr double kEpsilon = 1e-6;
   std::array<SceneBlock, SCENE_MAX_BLOCKS> mBlocks;
   std::array<SceneTiming, SCENE_MAX_SCENES> mTiming;
   int mCount = 0;
   SceneEndMode mEndMode = SceneEndMode::Stop;
};

#endif
