#include "ManagerWindow.h"


#include "CalCanvas.h"

#include "TExec.h"
#include "TRootEmbeddedCanvas.h"
#include "TApplication.h"

#include "TGNumberEntry.h"
#include "TGStatusBar.h"
#include "TGButton.h"
#include "TGProgressBar.h"

#include "base/Logger.h"

using namespace std;

namespace ant {
namespace calibration {
namespace gui {

class EmbeddedCanvas : public TRootEmbeddedCanvas {
public:
    EmbeddedCanvas(const TGWindow *p = 0) :
        TRootEmbeddedCanvas(0, p, 200, 200)
    {
        auto frame = (TGCompositeFrame*)fCanvasContainer;
        frame->RemoveInput(kKeyPressMask | kKeyReleaseMask);
    }
};

template<class theWidget>
class ActionWidget : public theWidget {
    struct MyExec : TExec {
        MyExec(function<void()> action_) : action(action_) {}
        virtual void Exec(const char*) override {
            action();
        }
    private:
        function<void()> action;
    };
    unique_ptr<MyExec> exec;

public:
    using theWidget::theWidget;

    void SetAction(function<void()> action) {
        if(exec)
            return;
        exec = std_ext::make_unique<MyExec>(action);
        TQObject::Connect("Clicked()", "TExec", exec.get(), "Exec(=\"\")");
    }
};

void ManagerWindow::CreateToolbar(TGVerticalFrame* frame)
{
    // first row  with loop control commands

    TGHorizontalFrame* frm1 = new TGHorizontalFrame(frame,200,40);

    auto btn_prev = new ActionWidget<TGTextButton>(frm1,"Prev (b)");
    keys[kKey_b] = btn_prev;

    auto btn_next = new ActionWidget<TGTextButton>(frm1,"Next (n)");
    keys[kKey_n] = btn_next;

    auto btn_goto = new ActionWidget<TGTextButton>(frm1,"Goto");

    TGNumberEntry* numberentry = new TGNumberEntry(frm1, 0, 3, -1,
                                                   TGNumberFormat::kNESInteger,
                                                   TGNumberFormat::kNEANonNegative
                                                   );

    auto btn_finish = new ActionWidget<TGTextButton>(frm1,"Finish Slice");

    auto btn_autocontinue = new ActionWidget<TGCheckButton>(frm1,"AutoContinue");
    btn_autocontinue->SetAction([this, btn_autocontinue] () {
        LOG(INFO) << btn_autocontinue->IsOn();
        Mode.alwaysDisplayFit = btn_autocontinue->IsOn();
    });

    auto btn_showfit = new ActionWidget<TGCheckButton>(frm1,"Show each fit");


    // second row with fit specific commands

    TGHorizontalFrame* frm2 = new TGHorizontalFrame(frame,200,40);

    auto btn_fit = new ActionWidget<TGTextButton>(frm2,"Fit (f)");
    keys[kKey_f] = btn_fit;

    auto btn_defaults = new ActionWidget<TGTextButton>(frm2,"SetDefaults (d)");
    keys[kKey_d] = btn_defaults;

    auto btn_undopop = new ActionWidget<TGTextButton>(frm2,"Undo pop (u)");
    keys[kKey_u] = btn_undopop;

    auto btn_undopush = new ActionWidget<TGTextButton>(frm2,"Undo push (i)");
    keys[kKey_i] = btn_undopush;

    auto layout_btn = new TGLayoutHints(kLHintsLeft,2,2,2,2);
    frm1->AddFrame(btn_prev, layout_btn);
    frm1->AddFrame(btn_next, layout_btn);
    frm1->AddFrame(btn_goto, layout_btn);
    frm1->AddFrame(numberentry, layout_btn);
    frm1->AddFrame(btn_finish, layout_btn);
    frm1->AddFrame(btn_autocontinue, layout_btn);
    frm1->AddFrame(btn_showfit, layout_btn);


    frm2->AddFrame(btn_fit, layout_btn);
    frm2->AddFrame(btn_defaults, layout_btn);
    frm2->AddFrame(btn_undopop, layout_btn);
    frm2->AddFrame(btn_undopush, layout_btn);


    // some progress bars

    TGHProgressBar* progress1 = new TGHProgressBar(frame);
    TGHProgressBar* progress2 = new TGHProgressBar(frame);

    auto layout_frm =  new TGLayoutHints(kLHintsTop | kLHintsExpandX, 0, 0, 0, 0);
    frame->AddFrame(frm1, layout_frm);
    frame->AddFrame(frm2, layout_frm);
    frame->AddFrame(progress1, layout_frm);
    frame->AddFrame(progress2, layout_frm);
}

ManagerWindow::ManagerWindow(const TGWindow* p, UInt_t w, UInt_t h) :
    TGMainFrame(p, w, h)
{

    TGVerticalFrame* frame = new TGVerticalFrame(this);

    // Create a horizontal frame widget with buttons
    CreateToolbar(frame);

    // Create frame for canvases
    frame_canvases = new TGHorizontalFrame(frame,200,40);
    frame->AddFrame(frame_canvases, new TGLayoutHints(kLHintsExpandY | kLHintsExpandX, 0, 0, 0, 0));

    // Statusbar
    Int_t parts[] = {45, 15, 10, 30};
    statusbar = new TGStatusBar(frame, 50, 10, kVerticalFrame);
    statusbar->SetParts(parts, 4);
    statusbar->Draw3DCorner(kFALSE);
    frame->AddFrame(statusbar, new TGLayoutHints(kLHintsTop | kLHintsExpandX, 0, 0, 10, 0));


    AddFrame(frame, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY, 0, 0, 0, 0));

    AddInput(kKeyPressMask | kKeyReleaseMask);

    // Set a name to the main frame
    SetWindowName("Ant-calib GUI");

    // Map all subwindows of main frame
    MapSubwindows();
    Resize(GetDefaultSize()); // this is used here to init layout algorithm
    MapWindow();

    // set focus
    gVirtualX->SetInputFocus(GetId());
}

Bool_t ManagerWindow::HandleKey(Event_t* event) {

    if (event->fType == kGKeyPress) {
        char input[10];
        UInt_t keysym;
        gVirtualX->LookupString(event, input, sizeof(input), keysym);

        auto it_key = keys.find((EKeySym)keysym);
        if(it_key != keys.end()) {
            it_key->second->Clicked();
            return kTRUE;
        }
    }
    return TGMainFrame::HandleKey(event);
}

CalCanvas* ManagerWindow::AddCalCanvas(const string& name) {
    auto ecanvas = new EmbeddedCanvas(frame_canvases);
    auto canvas = new gui::CalCanvas(name.c_str(),ecanvas->GetCanvasWindowId());
    canvas->ConnectStatusBar(statusbar);
    ecanvas->AdoptCanvas(canvas);
    frame_canvases->AddFrame(ecanvas, new TGLayoutHints(kLHintsExpandY | kLHintsExpandX, 0,0,0,0));
    MapSubwindows();
    Resize(GetDefaultSize());
    MapWindow();
    return canvas;
}

ManagerWindow::~ManagerWindow()
{
    gApplication->Terminate(0);
}



}}} // namespace ant::calibration::gui

