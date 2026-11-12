/***************************************************************
 ** Scene button strip for the extended Stochas fork.
 ** Licensed under the GNU General Public License v3 (see COPYING).
 ** Provided "AS IS" without warranty.
 ***************************************************************/
#ifndef SCENEPANEL_H_
#define SCENEPANEL_H_

#include "PluginProcessor.h"
#include <functional>

/* A strip of numbered buttons, one per scene. Click one to switch the plugin to
   that scene. The active scene is highlighted; scenes that have been used are
   brighter than untouched ones. */
class ScenePanel : public Component, private Button::Listener
{
public:
   explicit ScenePanel(SeqAudioProcessor &proc);

   void resized() override;

   // call regularly (from the editor's timer) to keep the highlight up to date
   void refresh();

   // called when the Song button is clicked
   std::function<void()> onSongButton;

private:
   void buttonClicked(Button *b) override;
   void applyColours(int idx, bool isCurrent, bool hasData);

   SeqAudioProcessor &mProc;
   Label mLabel;
   TextButton mBtn[SEQ_MAX_SCENES];
   TextButton mSongBtn;
   int mShownSong = -1;
   int mShownCurrent = -2;
   bool mShownHas[SEQ_MAX_SCENES];
};

#endif
