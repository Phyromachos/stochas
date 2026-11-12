/***************************************************************
 ** Song editor panel for the extended Stochas fork.
 ** Licensed under the GNU General Public License v3 (see COPYING).
 ** Provided "AS IS" without warranty.
 ***************************************************************/
#include "SongPanel.h"
#include "EditorState.h"

namespace {
   const int kRowH = 28;
   // column positions inside a row (also used to draw the column headings)
   const int kNumX = 0,     kNumW = 30;
   const int kSceneX = 34,  kSceneW = 70;
   const int kRepX = 112,   kRepW = 60;
   const int kStepsX = 180, kStepsW = 60;
   const int kAutoX = 248,  kAutoW = 130;
   const int kUpX = 386,    kBtnW = 30;
   const int kDownX = 420;
   const int kDelX = 454;
   const int kRowW = 490;
}

/*============================================
One row of the list = one song block
*============================================*/
class SongPanel::Row : public Component, private Label::Listener, private Button::Listener
{
public:
   Row(SongPanel &owner, int idx) : mIndex(idx), mOwner(owner)
   {
      mNum.setText(String(idx + 1), dontSendNotification);
      mNum.setJustificationType(Justification::centredRight);
      addAndMakeVisible(mNum);

      for (int i = 0; i < SEQ_MAX_SCENES; i++)
         mScene.addItem(String(i + 1), i + 1);
      mScene.onChange = [this] { commit(); };
      addAndMakeVisible(mScene);

      setupNumberLabel(mRepeats);
      setupNumberLabel(mSteps);

      mAuto.setJustificationType(Justification::centredLeft);
      mAuto.setColour(Label::textColourId, Colours::grey);
      addAndMakeVisible(mAuto);

      mUp.setButtonText("Up");
      mDown.setButtonText("Dn");
      mDel.setButtonText("X");
      for (TextButton *b : { &mUp, &mDown, &mDel }) {
         b->setWantsKeyboardFocus(false);
         b->addListener(this);
         addAndMakeVisible(*b);
      }
      update();
   }

   // pull the block's values from the song (leaves a box alone while it is being typed in)
   void update()
   {
      const SceneBlock b = mOwner.mProc.songGetBlock(mIndex);
      mScene.setSelectedId(b.scene + 1, dontSendNotification);
      if (!mRepeats.isBeingEdited())
         mRepeats.setText(String(b.repeats), dontSendNotification);
      if (!mSteps.isBeingEdited())
         mSteps.setText(String(b.steps), dontSendNotification);
      if (b.steps == 0)
         mAuto.setText("= " + String(mOwner.mProc.songGetSceneLoopSteps(b.scene)) + " steps (whole scene)", dontSendNotification);
      else
         mAuto.setText("", dontSendNotification);
   }

   void setPlaying(bool p)
   {
      if (p != mPlaying) {
         mPlaying = p;
         repaint();
      }
   }

   void paint(Graphics &g) override
   {
      g.setColour(mPlaying ? Colour(0xff7a5412) : Colour(0xff2a2f35));
      g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(0.0f, 1.0f), 3.0f);
   }

   void resized() override
   {
      const int y = 2, h = kRowH - 4;
      mNum.setBounds(kNumX, y, kNumW, h);
      mScene.setBounds(kSceneX, y, kSceneW, h);
      mRepeats.setBounds(kRepX, y, kRepW, h);
      mSteps.setBounds(kStepsX, y, kStepsW, h);
      mAuto.setBounds(kAutoX, y, kAutoW, h);
      mUp.setBounds(kUpX, y, kBtnW, h);
      mDown.setBounds(kDownX, y, kBtnW, h);
      mDel.setBounds(kDelX, y, kBtnW, h);
   }

private:
   void setupNumberLabel(Label &l)
   {
      l.setEditable(true, true, false);
      l.setJustificationType(Justification::centred);
      l.setColour(Label::backgroundColourId, Colour(0xff15181c));
      l.setColour(Label::outlineColourId, Colour(0xff55606b));
      l.setColour(Label::textColourId, Colours::white);
      l.addListener(this);
      addAndMakeVisible(l);
   }

   // send what is on screen to the song; if it isn't valid, put the old values back
   void commit()
   {
      const int scene = mScene.getSelectedId() - 1;
      const int reps = mRepeats.getText().trim().getIntValue();
      const int steps = mSteps.getText().trim().getIntValue();
      if (!mOwner.mProc.songSetBlock(mIndex, scene, reps, steps))
         update();
      else
         update();   // also refreshes the "= N steps" hint
   }

   void labelTextChanged(Label *) override { commit(); }

   void buttonClicked(Button *b) override
   {
      // the panel notices the change on its next refresh and redraws the list
      if (b == &mUp)
         mOwner.mProc.songMoveBlock(mIndex, mIndex - 1);
      else if (b == &mDown)
         mOwner.mProc.songMoveBlock(mIndex, mIndex + 1);
      else if (b == &mDel)
         mOwner.mProc.songRemoveBlock(mIndex);
   }

   int mIndex;
   SongPanel &mOwner;
   Label mNum, mRepeats, mSteps, mAuto;
   ComboBox mScene;
   TextButton mUp, mDown, mDel;
   bool mPlaying = false;
};

/*============================================
The panel
*============================================*/
SongPanel::SongPanel(SeqAudioProcessor &proc) : mProc(proc)
{
   mToggle.setClickingTogglesState(false);
   mToggle.setWantsKeyboardFocus(false);
   mToggle.onClick = [this] {
      mProc.songSetEnabled(!mProc.songIsEnabled());
      refresh();
   };
   addAndMakeVisible(mToggle);

   mEndLabel.setText("At the end:", dontSendNotification);
   mEndLabel.setJustificationType(Justification::centredRight);
   addAndMakeVisible(mEndLabel);

   mEnd.addItem("Stop", 1);
   mEnd.addItem("Loop the song", 2);
   mEnd.addItem("Hold the last block", 3);
   mEnd.onChange = [this] {
      const int id = mEnd.getSelectedId();
      mProc.songSetEndMode(id == 2 ? SceneEndMode::Loop : id == 3 ? SceneEndMode::HoldLast : SceneEndMode::Stop);
   };
   addAndMakeVisible(mEnd);

   mClose.setButtonText("Close");
   mClose.setWantsKeyboardFocus(false);
   mClose.onClick = [this] { if (onClose) onClose(); };
   addAndMakeVisible(mClose);

   mHint.setText("Plays these blocks in order, following the host's play position (the song starts at the "
                 "beginning of the timeline). Repeats = how many times the block plays. Steps = how long one "
                 "pass lasts; 0 means the scene's whole loop. Scene changes may land a few milliseconds late.",
                 dontSendNotification);
   mHint.setColour(Label::textColourId, Colours::grey);
   mHint.setFont(Font(FontOptions(12.0f)));
   addAndMakeVisible(mHint);

   mAdd.setButtonText("Add block");
   mAdd.setWantsKeyboardFocus(false);
   mAdd.onClick = [this] { mProc.songAddBlock(mProc.getCurrentScene(), 1, 0); };
   addAndMakeVisible(mAdd);

   mClear.setButtonText("Clear all");
   mClear.setWantsKeyboardFocus(false);
   mClear.onClick = [this] { mProc.songClear(); };
   addAndMakeVisible(mClear);

   mViewport.setViewedComponent(&mHolder, false);
   mViewport.setScrollBarsShown(true, false);
   addAndMakeVisible(mViewport);

   refresh();
}

SongPanel::~SongPanel()
{
   mViewport.setViewedComponent(nullptr, false);
}

void SongPanel::paint(Graphics &g)
{
   Colour bg = Colour(0xff1e2227);
   if (mProc.mEditorState)
      bg = mProc.mEditorState->getColorFor(EditorState::background);
   g.fillAll(bg);

   // column headings, lined up with the row columns
   g.setColour(Colours::grey);
   g.setFont(Font(FontOptions(12.0f)));
   const int x0 = mViewport.getX();
   g.drawText("#",       x0 + kNumX,   mHeaderY, kNumW,   16, Justification::centredRight);
   g.drawText("Scene",   x0 + kSceneX, mHeaderY, kSceneW, 16, Justification::centred);
   g.drawText("Repeats", x0 + kRepX,   mHeaderY, kRepW,   16, Justification::centred);
   g.drawText("Steps",   x0 + kStepsX, mHeaderY, kStepsW, 16, Justification::centred);
}

void SongPanel::resized()
{
   Rectangle<int> r = getLocalBounds().reduced(10);

   Rectangle<int> head = r.removeFromTop(30);
   mClose.setBounds(head.removeFromRight(80));
   head.removeFromRight(8);
   mToggle.setBounds(head.removeFromLeft(150));
   head.removeFromLeft(12);
   mEndLabel.setBounds(head.removeFromLeft(80));
   mEnd.setBounds(head.removeFromLeft(170));

   r.removeFromTop(6);
   mHint.setBounds(r.removeFromTop(48));
   mHeaderY = r.getY();
   r.removeFromTop(18);

   Rectangle<int> foot = r.removeFromBottom(30);
   mAdd.setBounds(foot.removeFromLeft(120));
   foot.removeFromLeft(8);
   mClear.setBounds(foot.removeFromLeft(100));
   r.removeFromBottom(6);

   mViewport.setBounds(r);
   layoutRows();
}

void SongPanel::layoutRows()
{
   const int w = jmax(kRowW, mViewport.getMaximumVisibleWidth());
   mHolder.setSize(w, jmax(1, mRows.size() * kRowH));
   for (int i = 0; i < mRows.size(); i++)
      mRows[i]->setBounds(0, i * kRowH, kRowW, kRowH);
}

void SongPanel::rebuildRows()
{
   mRows.clear();
   const int n = mProc.songGetBlockCount();
   for (int i = 0; i < n; i++) {
      Row *row = new Row(*this, i);
      mRows.add(row);
      mHolder.addAndMakeVisible(row);
   }
   mShownPlaying = -2;
   layoutRows();
}

void SongPanel::refresh()
{
   const unsigned v = mProc.songGetVersion();
   const int count = mProc.songGetBlockCount();

   if (v != mShownVersion || count != mRows.size()) {
      mShownVersion = v;
      mToggle.setButtonText(mProc.songIsEnabled() ? "Song playback: ON" : "Song playback: OFF");
      const SceneEndMode m = mProc.songGetEndMode();
      mEnd.setSelectedId(m == SceneEndMode::Loop ? 2 : m == SceneEndMode::HoldLast ? 3 : 1, dontSendNotification);

      if (count != mRows.size()) {
         rebuildRows();
      } else {
         for (Row *row : mRows)
            row->update();
      }
   }

   const int playing = mProc.songGetPlayingBlock();
   if (playing != mShownPlaying) {
      mShownPlaying = playing;
      for (int i = 0; i < mRows.size(); i++)
         mRows[i]->setPlaying(i == playing);
   }
}
