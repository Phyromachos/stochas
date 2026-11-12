/***************************************************************
 ** Scene button strip for the extended Stochas fork.
 ** Licensed under the GNU General Public License v3 (see COPYING).
 ** Provided "AS IS" without warranty.
 ***************************************************************/
#include "ScenePanel.h"

ScenePanel::ScenePanel(SeqAudioProcessor &proc) : mProc(proc)
{
   mLabel.setText("Scene", dontSendNotification);
   mLabel.setJustificationType(Justification::centredLeft);
   mLabel.setColour(Label::textColourId, Colours::grey);
   addAndMakeVisible(mLabel);

   mSongBtn.setButtonText("Song");
   mSongBtn.setWantsKeyboardFocus(false);
   mSongBtn.addListener(this);
   addAndMakeVisible(mSongBtn);

   for (int i = 0; i < SEQ_MAX_SCENES; i++) {
      mShownHas[i] = false;
      mBtn[i].setButtonText(String(i + 1));
      mBtn[i].setWantsKeyboardFocus(false);
      mBtn[i].addListener(this);
      addAndMakeVisible(mBtn[i]);
      applyColours(i, false, false);
   }
}

void ScenePanel::resized()
{
   Rectangle<int> r = getLocalBounds();
   mSongBtn.setBounds(r.removeFromRight(80).reduced(0, 3));
   r.removeFromRight(6);
   mLabel.setBounds(r.removeFromLeft(46));

   const int perRow = SEQ_MAX_SCENES / 2;
   const int rowH = 20;
   const int gap = 2;
   Rectangle<int> row1 = r.removeFromTop(rowH);
   r.removeFromTop(gap);
   Rectangle<int> row2 = r.removeFromTop(rowH);

   const int w = row1.getWidth() / perRow;
   for (int i = 0; i < SEQ_MAX_SCENES; i++) {
      const Rectangle<int> &row = (i < perRow) ? row1 : row2;
      const int col = i % perRow;
      mBtn[i].setBounds(row.getX() + col * w, row.getY(), w - 2, rowH);
   }
}

void ScenePanel::applyColours(int idx, bool isCurrent, bool hasData)
{
   Colour bg, txt;
   if (isCurrent) {
      bg = Colour(0xffe8a020);
      txt = Colours::black;
   } else if (hasData) {
      bg = Colour(0xff4a5a6a);
      txt = Colours::white;
   } else {
      bg = Colour(0xff2a2f35);
      txt = Colours::grey;
   }
   mBtn[idx].setColour(TextButton::buttonColourId, bg);
   mBtn[idx].setColour(TextButton::buttonOnColourId, bg);
   mBtn[idx].setColour(TextButton::textColourOffId, txt);
   mBtn[idx].setColour(TextButton::textColourOnId, txt);
   mBtn[idx].repaint();
}

void ScenePanel::refresh()
{
   const int songOn = mProc.songIsEnabled() ? 1 : 0;
   if (songOn != mShownSong) {
      mShownSong = songOn;
      mSongBtn.setButtonText(songOn ? "SONG ON" : "Song");
   }
   const int cur = mProc.getCurrentScene();
   for (int i = 0; i < SEQ_MAX_SCENES; i++) {
      const bool has = mProc.sceneHasData(i);
      const bool wasCur = (mShownCurrent == i);
      const bool isCur = (cur == i);
      // the first time through mShownCurrent is -2, so everything is painted once
      if (mShownCurrent == -2 || has != mShownHas[i] || wasCur != isCur) {
         mShownHas[i] = has;
         applyColours(i, isCur, has);
      }
   }
   mShownCurrent = cur;
}

void ScenePanel::buttonClicked(Button *b)
{
   if (b == &mSongBtn) {
      if (onSongButton)
         onSongButton();
      return;
   }
   for (int i = 0; i < SEQ_MAX_SCENES; i++) {
      if (b == &mBtn[i]) {
         mProc.selectScene(i);
         refresh();
         return;
      }
   }
}
