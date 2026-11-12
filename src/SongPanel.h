/***************************************************************
 ** Song editor panel for the extended Stochas fork.
 ** Licensed under the GNU General Public License v3 (see COPYING).
 ** Provided "AS IS" without warranty.
 ***************************************************************/
#ifndef SONGPANEL_H_
#define SONGPANEL_H_

#include "PluginProcessor.h"
#include <functional>

/* A panel that covers the main editor area (the scene strip stays visible above it).
   It shows the song as a list of blocks. Each block has a scene, a number of repeats and
   a number of steps per pass (0 = play the scene's whole loop). The block that is
   playing right now is highlighted. */
class SongPanel : public Component
{
public:
   explicit SongPanel(SeqAudioProcessor &proc);
   ~SongPanel() override;

   void paint(Graphics &g) override;
   void resized() override;

   // call regularly (from the editor's timer) while the panel is showing
   void refresh();

   std::function<void()> onClose;

private:
   class Row;
   friend class Row;

   void rebuildRows();
   void layoutRows();

   SeqAudioProcessor &mProc;
   TextButton mToggle;
   Label mEndLabel;
   ComboBox mEnd;
   TextButton mClose;
   Label mHint;
   TextButton mAdd;
   TextButton mClear;
   Viewport mViewport;
   Component mHolder;
   OwnedArray<Row> mRows;
   int mHeaderY = 0;
   unsigned mShownVersion = 0xffffffffu;
   int mShownPlaying = -2;
};

#endif
