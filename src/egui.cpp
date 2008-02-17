/*    egui.cpp
 *
 *    Copyright (c) 2008, eFTE SF Group (see AUTHORS file)
 *    Copyright (c) 1994-1996, Marko Macek
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

#include "fte.h"
#include "log.h"

int LastEventChar = -1;

EFrame::EFrame(int XSize, int YSize): GFrame(XSize, YSize) {
    CMap = 0;
    CModel = 0;
}

EFrame::~EFrame() {
}

void EFrame::Update() {
    GxView *V = (GxView *)Active;

    if (V) {
        if (CModel != ActiveModel && ActiveModel) {
            char Title[256] = ""; //fte: ";
            char STitle[256] = ""; //"fte: ";
            ActiveModel->GetTitle(Title, sizeof(Title) - 1,
                                  STitle, sizeof(STitle) - 1);
            ConSetTitle(Title, STitle);
            CModel = ActiveModel;
        }
    }
    GFrame::Update();
}

void EFrame::UpdateMenu() {
    GxView *V = (GxView *)Active;
    EEventMap *Map = 0;

    if (V)
        Map = V->GetEventMap();

    if (Map != CMap || CMap == 0) {
        const char *NMenu = 0;
        const char *OMenu = 0;
        // set menu

        if (CMap)
            OMenu = CMap->GetMenu(EM_MainMenu);
        if (Map)
            NMenu = Map->GetMenu(EM_MainMenu);
        if (NMenu == 0)
            NMenu = "Main";
        CMap = Map;

        if (OMenu && strcmp(OMenu, NMenu) == 0) {
            // ok
        } else {
            SetMenu(NMenu);
        }
    } /*else if (CMap == 0 && Map == 0) {
        SetMenu("Main");
    }*/

    GFrame::UpdateMenu();
}

EGUI::EGUI(int &argc, char **argv, int XSize, int YSize)
        : GUI(argc, argv, XSize, YSize) {
    ActiveMap = 0;
    OverrideMap = 0;
    CharMap[0] = 0;
}

EGUI::~EGUI() {
}

/**
 * Push a number/char from a macro onto the stack
 */
int Push(GxView *view, ExState &State, int repeat) {
    ExModelView *V = (ExModelView *)view->Top;
    EView *View = V->View;
    int found;
    int x = 0;
    char s[64] = "";

    do {
        found = 0;
        if (State.GetIntParam(View, &x)) {
            ParamStack.push(x);
            found = 1;
        } else if (State.GetStrParam(View, s, sizeof(s))) {
            ParamStack.push((int) s[0]);
            found = 1;
        }
    } while(found == 1);

    SetBranchCondition(1);
    return 1;
}

int EGUI::ExecCommand(GxView *view, int Command, ExState &State) {
    if (Command & CMD_EXT)
        return ExecMacro(view, Command & ~CMD_EXT);

    if (Command == ExFail)
        return ErFAIL;

    if (view->IsModelView()) {
        ExModelView *V = (ExModelView *)view->Top;
        EView *View = V->View;

        switch (Command) {
        case ExFileClose:
            return FileClose(View, State);
        case ExFileCloseAll:
            return FileCloseAll(View, State);
        case ExExitEditor:
            return ExitEditor(View);
        case ExIncrementalSearch:
            return View->MView->Win->IncrementalSearch(View);
        }
    }
    switch (Command) {
    case ExPush:
        return Push(view, State, Macros[State.Macro].cmds[State.Pos-1].repeat);
    case ExWinRefresh:
        view->Repaint();
        return 1;
    case ExWinNext:
        return WinNext(view);
    case ExWinPrev:
        return WinPrev(view);
    case ExShowEntryScreen:
        return ShowEntryScreen();
    case ExRunProgram:
        return RunProgram(State, view);
    case ExRunProgramAsync:
        return RunProgramAsync(State, view);
    case ExMainMenu:
        return MainMenu(State, view);
    case ExShowMenu:
        return ShowMenu(State, view);
    case ExLocalMenu:
        return LocalMenu(view);
    case ExFrameNew:
        return FrameNew();
    case ExFrameNext:
        return FrameNext(view);
    case ExFramePrev:
        return FramePrev(view);
    case ExWinHSplit:
        return WinHSplit(view);
    case ExWinClose:
        return WinClose(view);
    case ExWinZoom:
        return WinZoom(view);
    case ExWinResize:
        return WinResize(State, view);
    case ExDesktopSaveAs:
        return DesktopSaveAs(State, view);
    case ExDesktopSave:
        if (DesktopFileName[0] != 0)
            return SaveDesktop(DesktopFileName);
        return 0;
    case ExDesktopLoad:
        return DesktopLoad(State, view);
    case ExChangeKeys: {
        char kmaps[64] = "";
        EEventMap *m;

        if (State.GetStrParam(0, kmaps, sizeof(kmaps)) == 0) {
            SetOverrideMap(0, 0);
            return 0;
        }
        m = FindEventMap(kmaps);
        if (m == 0)
            return 0;
        SetOverrideMap(m->KeyMap, m->Name);
        return 1;
    }
    }

    //fprintf(stderr, "ExecCommand: %d\n",State);
    return view->ExecCommand(Command, State);
}

int EGUI::BeginMacro(GxView *view) {
    view->BeginMacro();
    return 1;
}

int TestBranchCondition()  {
    return (ParamStack.pop());
}

int EGUI::ExecMacro(GxView *view, int Macro) {
    STARTFUNC("EGUI::ExecMacro");

    int i, j, tos, ResultOfCommandExecution;
    ExMacro *m;
    ExState State;
    if (Macro == -1)  {
        ENDFUNCRC(ErFAIL);
    }
    if (BeginMacro(view) == -1) {
        ENDFUNCRC(ErFAIL);
    }
    State.Pos = 0;
    State.Macro = Macro;
    m = &Macros[State.Macro];

    for (i=State.Pos; i < m->Count; i++) {

        if (m->cmds[i].type != CT_COMMAND) {
            LOG << "not CT_COMMAND" << ENDLINE;
            continue;
        }
        if (m->cmds[i].u.num == ExNop)  {
            LOG << "Nop" << ENDLINE;
            continue;
        }
        switch (m->cmds[i].u.num) {
        case ExExit:             // works now as exit: early terminate a macro
            i = m->Count;
            //fprintf(stderr,"macro exit at i=%d\n",i-1);
            LOG << "macro exit" << ENDLINE;
            break;

        case ExUnconditionalBranch:
            //fprintf(stderr,"tos=%d, nos=%d, 3rd=%d -- ",ParamStack.peek(0),ParamStack.peek(1),ParamStack.peek(2));
            //fprintf(stderr,"branch at %d",i);
            i += m->cmds[i].repeat;
            //fprintf(stderr," to %d\n",i+1);
            LOG << "branch to " << i+1 << ENDLINE;
            break;

        case ExConditionalBranch:
            //fprintf(stderr,"tos=%d, nos=%d, 3rd=%d -- ",ParamStack.peek(0),ParamStack.peek(1),ParamStack.peek(2));
            //fprintf(stderr,"conditional branch at %d ",i);
            if (TestBranchCondition()) {
                //fprintf(stderr,"not ");
                LOG << "cond branch not taken. proceeding @ " << i+1 << ENDLINE;
            } else {
                i += m->cmds[i].repeat;
                LOG << "cond branch to " << i+1 << ENDLINE;
            }
            //fprintf(stderr," taken, proceeding @ %d\n",i+1);
            break;

        case ExDoRuntime:                                           // do loop setup code: initialize loop
            //fprintf(stderr,"tos=%d, nos=%d, 3rd=%d\n",ParamStack.peek(0),ParamStack.peek(1),ParamStack.peek(2));
            //fprintf(stderr,"DO at %d\n",i);
            if (ParamStack.peek(0) == ParamStack.peek(1)) {         // limit and start identical? skip loop then:
                ParamStack.pop();                                   //    drop start
                ParamStack.pop();                                   //    drop limit
                i += m->cmds[i].repeat;                             //    proceed at behind LOOP
                //fprintf(stderr,"skipping loop, proceeding at %d\n",i);
            } else {                                                // will have to loop:
                tos = ParamStack.pop();                             //    pop start to temp
                ControlStack.push(ParamStack.pop());                //    stack limit
                ControlStack.push(tos);                             //    stack start
                //fprintf(stderr,"entering loop\n");
            }                                                       //    proceed with loop body
            break;

        case ExLoopRuntime:                                         // executed once per loop iteration:
            //fprintf(stderr,"tos=%d, nos=%d, 3rd=%d\n",ParamStack.peek(0),ParamStack.peek(1),ParamStack.peek(2));
            tos = ControlStack.pop()+1;                             //    increment loop index
            //fprintf(stderr,"LOOP at %d, index=%d\n",i,tos);
            if (ControlStack.peek(0) == tos)  {                     //    reached limit?
                //fprintf(stderr,"LOOP limit reached\n");
                ControlStack.pop();                                 //    yes: clean up
            } else {                                                //    no:
                ControlStack.push(tos);                             //       keep loop index for next round
                i += m->cmds[i].repeat;                             //       branch back to behind DO
                //fprintf(stderr,"LOOPing back to %d\n",i);
            }
            break;

        case ExLeaveRuntime:                                        // executed once per loop iteration:
            ControlStack.pop();
            ControlStack.pop();
            //fprintf(stderr,"LEAVE at %d, ",i);
            i += m->cmds[i].repeat;
            //fprintf(stderr,"DO at %d, ",i);
            i += m->cmds[i].repeat;
            //fprintf(stderr,"LOOP at %d\n",i);
            break;


        case ExTimes:
            m->cmds[i+1].repeat = ParamStack.pop();
            break;


        default:
            for (j=(m->cmds[i].repeat); j; --j) {
                State.Pos = i + 1;
                ResultOfCommandExecution=ExecCommand(view, m->cmds[i].u.num, State);

                if (!(ResultOfCommandExecution || m->cmds[i].ign)) {
                    LOG << "ExecCommand fail: result=" << ResultOfCommandExecution << ", ign=" << m->cmds[i].ign << ENDLINE;
                    return ErFAIL;
                }
                if (ResultOfCommandExecution>>1) {
                    LOG << "ExecCommand shouldn't but did return " << ResultOfCommandExecution << ENDLINE;
                }
            }
        }
       State.Pos=i;
    }
    ENDFUNCRC(ErOK);
}




void EGUI::SetMsg(char *Msg) {
    char CharMap[128] = "";

    if (Msg == 0) {
        strcpy(CharMap, "");
    } else {
        strcat(CharMap, "[");
        strcat(CharMap, Msg);
        strcat(CharMap, "]");
    }
    if (ActiveModel)
        ActiveModel->Msg(S_INFO, CharMap);
}

void EGUI::SetOverrideMap(EKeyMap *aMap, char *ModeName) {
    OverrideMap = aMap;
    if (aMap == 0)
        SetMsg(0);
    else
        SetMsg(ModeName);
}

void EGUI::SetMap(EKeyMap *aMap, KeySel *ks) {
    char key[32] = "";

    ActiveMap = aMap;
    if (ActiveMap == 0) {
        SetMsg(0);
    } else {
        if (ks != 0) {
            GetKeyName(key, sizeof(key), *ks);
            SetMsg(key);
        }
    }
}

void EGUI::DispatchKey(GxView *view, TEvent &Event) {
    EEventMap *EventMap;
    EKeyMap *map;
    EKey *key = 0;
    char Ch;

    if (Event.Key.Code & kfModifier)
        return;

    LastEventChar = -1;
    if (GetCharFromEvent(Event, &Ch))
        LastEventChar = Ch;

    if ((EventMap = view->GetEventMap()) == 0)
        return;

    map = EventMap->KeyMap;

    if (ActiveMap || OverrideMap) {
        map = ActiveMap;
        if (OverrideMap)
            map = OverrideMap;
        while (map) {
            if ((key = map->FindKey(Event.Key.Code)) != 0) {
                if (key->fKeyMap) {
                    SetMap(key->fKeyMap, &key->fKey);
                    Event.What = evNone;
                    return ;
                } else {
                    SetMap(0, &key->fKey);
                    ExecMacro(view, key->Cmd);
                    Event.What = evNone;
                    return ;
                }
            }
            map = map->fParent;
        }
        if (!OverrideMap) {
            SetMap(0, 0);
            Event.What = evNone;
        }
        return ;
    }
    while (EventMap) {
        if (map) {
            if ((key = map->FindKey(Event.Key.Code)) != 0) {
                if (key->fKeyMap) {
                    SetMap(key->fKeyMap, &key->fKey);
                    Event.What = evNone;
                    return ;
                } else {
                    ExecMacro(view, key->Cmd);
                    Event.What = evNone;
                    return ;
                }
            }
        }
        EventMap = EventMap->Parent;
        if (EventMap == 0) break;
        map = EventMap->KeyMap;
    }
    //if (GetCharFromEvent(Event, &Ch))
    //    CharEvent(view, Event, Ch);
    SetMap(0, 0);
}

void EGUI::DispatchCommand(GxView *view, TEvent &Event) {
    if (Event.Msg.Command > 65536 + 16384) { // hack for PM toolbar
        Event.Msg.Command -= 65536 + 16384;
        BeginMacro(view);
        ExState State;
        State.Macro = 0;
        State.Pos = 0;
        ExecCommand(view, Event.Msg.Command, State);
        Event.What = evNone;
    } else if (Event.Msg.Command >= 65536) {
        Event.Msg.Command -= 65536;
        ExecMacro(view, Event.Msg.Command);
        Event.What = evNone;
    }
}

void EGUI::DispatchEvent(GFrame *frame, GView *view, TEvent &Event) {
    GxView *xview = (GxView *) view;

    if (Event.What == evNone ||
            (Event.What == evMouseMove && Event.Mouse.Buttons == 0))
        return ;

    if (Event.What == evNotify && Event.Msg.Command == cmPipeRead) {
        Event.Msg.Model->NotifyPipe(Event.Msg.Param1);
        return;
    }
    if (xview->GetEventMap() != 0) {
        switch (Event.What) {
        case evKeyDown:
            DispatchKey(xview, Event);
            break;
        case evCommand:
            if (Event.Msg.Command >= 65536) {
                DispatchCommand(xview, Event);
            } else {
                switch (Event.Msg.Command) {
                case cmClose: {
                    assert(ActiveView != 0);
                    FrameClose(ActiveView->MView->Win);
                    return;
                }
                }
            }
        }
    }
    GUI::DispatchEvent(frame, view, Event);
#if defined(OS2) && !defined(DBMALLOC) && defined(CHECKHEAP)
    if (_heapchk() != _HEAPOK)
        DieError(0, "Heap memory is corrupt.");
#endif
}

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

int EGUI::WinNext(GxView *view) {
    view->Parent->SelectNext(0);
    return 1;
}

int EGUI::WinPrev(GxView *view) {
    view->Parent->SelectNext(1);
    return 1;
}

int EGUI::FileCloseX(EView *View, int CreateNew, int XClose) {
    char Path[MAXPATH];

    // this should never fail!
    if (GetDefaultDirectory(View->Model, Path, sizeof(Path)) == 0)
        return 0;

    if (View->Model->ConfQuit(View->MView->Win)) {

        View->Model->DeleteRelated();

        // close everything that can be closed without confirmation if closing all
        if (XClose)
            while (View->Model->Next != View->Model &&
                    View->Model->Next->CanQuit())
                delete View->Model->Next;

        View->DeleteModel(View->Model);

        if (ActiveModel == 0 && CreateNew) {
            EView *V = ActiveView;
            EModel *m = new EDirectory(0, &ActiveModel, Path);
            if (m == 0) {
                View->MView->Win->Choice(GPC_ERROR, "Error", 1, "O&K", "Could not create directory view");
                return 0;
            }

            do {
                V = V->Next;
                V->SelectModel(ActiveModel);
            } while (V != ActiveView);
            return 0;
        }

        if (ActiveModel == 0) {
            StopLoop();
        }
        return 1;
    }
    return 0;
}


int EGUI::FileClose(EView *View, ExState &State) {
    int x = 0;

    if (State.GetIntParam(View, &x) == 0)
        x = OpenAfterClose;

    return FileCloseX(View, x);
}

int EGUI::FileCloseAll(EView *View, ExState &State) {
    int x = 0;

    if (State.GetIntParam(View, &x) == 0)
        x = OpenAfterClose;

    while (ActiveModel)
        if (FileCloseX(View, x, 1) == 0) return 0;
    return 1;
}

int EGUI::WinHSplit(GxView *View) {
    GxView *view;
    ExModelView *edit;
    EView *win;
    int W, H;

    View->ConQuerySize(&W, &H);

    if (H < 8)
        return 0;
    view = new GxView(View->Parent);
    if (view == 0)
        return 0;
    win = new EView(ActiveModel);
    if (win == 0)
        return 0;
    edit = new ExModelView(win);
    if (edit == 0)
        return 0;
    view->PushView(edit);
    view->Parent->SelectNext(0);
    return 1;
}

int EGUI::WinClose(GxView * /*V*/) {
    EView *View = ActiveView;

    if (View->Next == View) {
        // when closing last window, close all files
        if (ExitEditor(View) == 0)
            return 0;
    } else {
        View->MView->Win->Parent->SelectNext(0);
        delete View->MView->Win;
    }
    return 1;
}

int EGUI::WinZoom(GxView *View) {
    GView *V = View->Next;
    GView *V1;

    while (V) {
        V1 = V;
        if (V == View)
            break;
        V = V->Next;
        delete V1;
    }
    return 1;
}

int EGUI::WinResize(ExState &State, GxView *View) {
    int Delta = 1;

    if (State.GetIntParam(0, &Delta)) {
        if (View->ExpandHeight(Delta) == 0)
            return 1;
    }
    return 0;
}

int EGUI::ExitEditor(EView *View) {
    EModel *B = ActiveModel;

    // check/save modified files
    while (ActiveModel) {
        if (ActiveModel->CanQuit()) ;
        else {
            View->SelectModel(ActiveModel);
            int rc = ActiveModel->ConfQuit(View->MView->Win, 1);
            if (rc == -2) {
                View->FileSaveAll();
                break;
            }
            if (rc == 0)
                return 0;
        }

        ActiveModel = ActiveModel->Next;
        if (ActiveModel == B)
            break;
    }

    if (SaveDesktopOnExit && DesktopFileName[0] != 0)
        SaveDesktop(DesktopFileName);
    else if (LoadDesktopMode == 2) {       // Ask about saving?
        GxView* gx = View->MView->Win;

        if (gx->GetStr("Save desktop As",
                       sizeof(DesktopFileName), DesktopFileName,
                       HIST_DEFAULT) != 0) {
            SaveDesktop(DesktopFileName);
        }
    }

    while (ActiveModel) {
        if (View->Model->GetContext() == CONTEXT_ROUTINES) { // Never delete Routine models directly
            ActiveModel = ActiveModel->Next;
            View->SelectModel(ActiveModel);
        }

        View->Model->DeleteRelated();  // delete related views first

        while (View->Model->Next != View->Model &&
                View->Model->Next->CanQuit())
            delete View->Model->Next;

        View->DeleteModel(View->Model);
    }

    StopLoop();
    return 1;
}

int EGUI::ShowEntryScreen() {
    return gui->ShowEntryScreen();
}

int EGUI::RunProgram(ExState &State, GxView *view) {
    static char Cmd[512] = "";

    if (ActiveModel)
        SetDefaultDirectory(ActiveModel);

    if (State.GetStrParam(ActiveView, Cmd, sizeof(Cmd)) == 0)
        if (view->GetStr("Run", sizeof(Cmd), Cmd, HIST_COMPILE) == 0) return 0;
    gui->RunProgram(RUN_WAIT, Cmd);
    return 1;
}

int EGUI::RunProgramAsync(ExState &State, GxView *view) {
    static char Cmd[512] = "";

    if (ActiveModel)
        SetDefaultDirectory(ActiveModel);

    if (State.GetStrParam(ActiveView, Cmd, sizeof(Cmd)) == 0)
        if (view->GetStr("Run", sizeof(Cmd), Cmd, HIST_COMPILE) == 0) return 0;
    gui->RunProgram(RUN_ASYNC, Cmd);
    return 1;
}

int EGUI::MainMenu(ExState &State, GxView *View) {
    char s[3];

    if (State.GetStrParam(0, s, sizeof(s)) == 0)
        s[0] = 0;

    View->Parent->ExecMainMenu(s[0]);
    return 1;
}

int EGUI::ShowMenu(ExState &State, GxView *View) {
    char MName[32] = "";

    if (State.GetStrParam(0, MName, sizeof(MName)) == 0)
        return 0;

    View->Parent->PopupMenu(MName);
    return 0;
}

int EGUI::LocalMenu(GxView *View) {
    EEventMap *Map = View->GetEventMap();
    const char *MName = 0;

    if (Map)
        MName = Map->GetMenu(EM_LocalMenu);
    if (MName == 0)
        MName = "Local";
    View->Parent->PopupMenu(MName);
    return 0;
}

int EGUI::DesktopSaveAs(ExState &State, GxView *view) {
    if (State.GetStrParam(0, DesktopFileName, sizeof(DesktopFileName)) == 0)
        if (view->GetFile("Save Desktop", sizeof(DesktopFileName), DesktopFileName, HIST_PATH, GF_SAVEAS) == 0)
            return 0;

    if (DesktopFileName[0] != 0)
        return SaveDesktop(DesktopFileName);
    return 0;
}

int EGUI::DesktopLoad(ExState &State, GxView *view) {
    if (State.GetStrParam(0, DesktopFileName, sizeof(DesktopFileName)) == 0)
        if (view->GetFile("Load Desktop", sizeof(DesktopFileName), DesktopFileName, HIST_PATH, GF_OPEN) == 0)
            return 0;

    if (DesktopFileName[0] != 0)
        return LoadDesktop(DesktopFileName);
    return 0;
}

int EGUI::FrameNew() {
    GxView *view;
    ExModelView *edit;

    if (!multiFrame() && frames)
        return 0;

    (void) new EFrame(ScreenSizeX, ScreenSizeY);
    assert(frames != 0);

    //frames->SetMenu("Main"); //??

    view = new GxView(frames);
    assert(view != 0);

    (void)new EView(ActiveModel);
    assert(ActiveView != 0);

    edit = new ExModelView(ActiveView);
    assert(edit != 0);
    view->PushView(edit);
    frames->Show();
    return 1;
}

int EGUI::FrameClose(GxView *View) {
    assert(frames != 0);
    assert(View != 0);

    if (!frames->isLastFrame()) {
        deleteFrame(frames);
    } else {
        if (ExitEditor(ActiveView) == 0)
            return 0;
        deleteFrame(frames);
    }
    return 1;
}

int EGUI::FrameNext(GxView * /*View*/) {
    if (!frames->isLastFrame()) {
        frames->Next->Activate();
        return 1;
    }
    return 0;
}

int EGUI::FramePrev(GxView * /*View*/) {
    if (!frames->isLastFrame()) {
        frames->Prev->Activate();
        return 1;
    }
    return 0;
}

int EGUI::findDesktop(char *argv[]) {
    /*
     *  Locates the desktop file depending on the load desktop mode flag:
     *  0:  Try the "current" directory, then the FTE .exe directory (PC) or
     *      the user's homedir (Unix). Fail if not found (original FTE
     *      algorithm).
     *  1:  Try the current directory, then loop {go one directory UP and try}
     *      If not found, don't load a desktop!
     *  2:  As above, but asks to save the desktop if one was not found.
     *  This is called if the desktop is not spec'd on the command line.
     */
    switch (LoadDesktopMode) {
    default:
        //** 0: try curdir then "homedir"..
        //         fprintf(stderr, "ld: Mode 0\n");
        if (FileExists(DESKTOP_NAME))
            ExpandPath(DESKTOP_NAME, DesktopFileName, sizeof(DesktopFileName));
        else {
            //** Use homedir,
#ifdef UNIX
            ExpandPath("~/" DESKTOP_NAME, DesktopFileName, sizeof(DesktopFileName));
#else
            JustDirectory(argv[0], DesktopFileName, sizeof(DesktopFileName));
            strlcat(DesktopFileName, DESKTOP_NAME, sizeof(DesktopFileName));
#endif // UNIX
        }
        return FileExists(DesktopFileName);

    case 1:
    case 2:
        //** Try curdir, then it's owner(s)..
        ExpandPath(".", DesktopFileName, sizeof(DesktopFileName));
        //fprintf(stderr, "ld: Mode 1 (start at %s)\n", DesktopFileName);

        for (;;) {
            //** Try current location,
            char *pe = DesktopFileName + strlen(DesktopFileName);
            Slash(DesktopFileName, 1);      // Add appropriate slash
            strlcat(DesktopFileName, DESKTOP_NAME, sizeof(DesktopFileName));

            //fprintf(stderr, "ld: Mode 1 (trying %s)\n", DesktopFileName);
            if (FileExists(DesktopFileName)) {
                //fprintf(stderr, "ld: Mode 1 (using %s)\n", DesktopFileName);
                return 1;
            }

            //** Not found. Remove added stuff, then go level UP,
            *pe = 0;

            // Remove the current part,
            char *p = SepRChr(DesktopFileName);

            if (p == NULL) {
                //** No desktop! Set default name in current directory,
                ExpandPath(".", DesktopFileName, sizeof(DesktopFileName));
                Slash(DesktopFileName, 1);
                strlcat(DesktopFileName, DESKTOP_NAME, sizeof(DesktopFileName));

                SaveDesktopOnExit = 0;      // Don't save,
                return 0;                   // NOT found!!
            }
            *p = 0;                         // Truncate name at last
        }
    }
}

void EGUI::DoLoadDesktopOnEntry(int &/*argc*/, char **argv) {
    if (DesktopFileName[0] == 0)
        findDesktop(argv);

    if (DesktopFileName[0] != 0) {
        if (IsDirectory(DesktopFileName)) {
            Slash(DesktopFileName, 1);
            strlcat(DesktopFileName, DESKTOP_NAME, sizeof(DesktopFileName));
        }

        if (LoadDesktopOnEntry && FileExists(DesktopFileName))
            LoadDesktop(DesktopFileName);
    }
}

void EGUI::EditorInit() {
    SSBuffer = new EBuffer(0, (EModel **)&SSBuffer, "Scrap");
    assert(SSBuffer != 0);
    BFI(SSBuffer, BFI_Undo) = 0; // disable undo for clipboard
    ActiveModel = 0;
}

int EGUI::InterfaceInit(int &/*argc*/, char ** /*argv*/) {
    if (FrameNew() == 0)
        DieError(1, "Failed to create window\n");
    return 0;
}

void EGUI::DoLoadHistoryOnEntry(int &/*argc*/, char **argv) {
    if (HistoryFileName[0] == 0) {
#ifdef UNIX
        ExpandPath("~/.efte-history", HistoryFileName, sizeof(HistoryFileName));
#else
        JustDirectory(argv[0], HistoryFileName, sizeof(HistoryFileName));
        strlcat(HistoryFileName, "efte.his", sizeof(HistoryFileName));
#endif // UNIX
    } else {
        char p[256];

        ExpandPath(HistoryFileName, p, sizeof(p));
        if (IsDirectory(p)) {
            Slash(p, 1);
            strlcat(p, HISTORY_NAME, sizeof(p));
        }
        strlcpy(HistoryFileName, p, sizeof(HistoryFileName));
    }

    if (KeepHistory && FileExists(HistoryFileName))
        LoadHistory(HistoryFileName);
}

void EGUI::DoSaveHistoryOnExit() {
    if (KeepHistory && HistoryFileName[0] != 0)
        SaveHistory(HistoryFileName);

    // since we are exiting, free history
    ClearHistory();
}

int EGUI::CmdLoadFiles(int &argc, char **argv) {
    int QuoteNext = 0;
    int QuoteAll = 0;
    int GotoLine = 0;
    int LineNum = 1;
    int ColNum = 1;
    int ModeOverride = 0;
    char Mode[32];
    int LCount = 0;
    int ReadOnly = 0;

    for (int Arg = 1; Arg < argc; Arg++) {
        if (!QuoteAll && !QuoteNext && (argv[Arg][0] == '-')) {
            switch (argv[Arg][1]) {
            case '-':
                if (strncmp(argv[Arg], "--debug", 7) != 0)
                    QuoteAll = 1;
                Arg = argc;
                break;
            case '!':
            case 'C':
            case 'c':
            case 'D':
            case 'd':
            case 'H':
                // handled before
                break;
            case '+':
                QuoteNext = 1;
                break;
            case '#':
            case 'l':
                LineNum = 1;
                ColNum = 1;
                if (strchr(argv[Arg], ',')) {
                    GotoLine = (2 == sscanf(argv[Arg] + 2, "%d,%d", &LineNum, &ColNum));
                } else {
                    GotoLine = (1 == sscanf(argv[Arg] + 2, "%d", &LineNum));
                }
                // printf("Gotoline = %d, line = %d, col = %d\n", GotoLine, LineNum, ColNum);
                break;
            case 'r':
                ReadOnly = 1;
                break;
            case 'm':
                if (argv[Arg][2] == 0) {
                    ModeOverride = 0;
                } else {
                    ModeOverride = 1;
                    strcpy(Mode, argv[Arg] + 2);
                }
                break;
            case 'T':
                TagsAdd(argv[Arg] + 2);
                break;
            case 't':
                TagGoto(ActiveView, argv[Arg] + 2);
                break;
            default:
                DieError(2, "Invalid command line option %s", argv[Arg]);
                return 0;
            }
        } else {
            char Path[MAXPATH];

            QuoteNext = 0;
            if (ExpandPath(argv[Arg], Path, sizeof(Path)) == 0 && IsDirectory(Path)) {
                EModel *m = new EDirectory(cfAppend, &ActiveModel, Path);
                if (m == 0 || ActiveModel == 0) {
                    DieError(2, "Could not open a directory view of path: %s\n", Path);
                }
            } else {
                if (LCount != 0)
                    suspendLoads = 1;
                if (MultiFileLoad(cfAppend, argv[Arg],
                                  ModeOverride ? Mode : 0,
                                  ActiveView) == 0) {
                    suspendLoads = 0;
                    return 0;
                }
                suspendLoads = 0;

                if (GotoLine) {
                    if (((EBuffer *)ActiveModel)->Loaded == 0)
                        ((EBuffer *)ActiveModel)->Load();
                    if (GotoLine) {
                        GotoLine = 0;
                        ((EBuffer *)ActiveModel)->SetNearPosR(ColNum - 1, LineNum - 1);
                    } else {
                        int r, c;

                        if (RetrieveFPos(((EBuffer *)ActiveModel)->FileName, r, c) == 1)
                            ((EBuffer *)ActiveModel)->SetNearPosR(c, r);
                    }
                    //ActiveView->SelectModel(ActiveModel);
                }
                if (ReadOnly) {
                    ReadOnly = 0;
                    BFI(((EBuffer *)ActiveModel), BFI_ReadOnly) = 1;
                }
            }
            suspendLoads = 1;
            ActiveView->SelectModel(ActiveModel->Next);
            suspendLoads = 0;
            LCount++;
        }
    }
    EModel *P = ActiveModel;
    while (LCount-- > 0)
        P = P->Prev;
    ActiveView->SelectModel(P);
    return 1;
}

int EGUI::Start(int &argc, char **argv) {
    {
        int rc;

        rc = GUI::Start(argc, argv);

        if (rc)
            return rc;
    }

    if (InterfaceInit(argc, argv) != 0)
        return 2;

    EditorInit();

    DoLoadHistoryOnEntry(argc, argv);
    DoLoadDesktopOnEntry(argc, argv);

    if (CmdLoadFiles(argc, argv) == 0)
        return 3;

    if (ActiveModel == 0) {
        char Path[MAXPATH];

        GetDefaultDirectory(0, Path, sizeof(Path));
        EModel *m = new EDirectory(0, &ActiveModel, Path);
        if (m == 0 || ActiveModel == 0) {
            DieError(2, "Could not open a directory view of path: %s\n", Path);
        }

        ActiveView->SwitchToModel(ActiveModel);
    }
    return 0;
}

void EGUI::EditorCleanup() {
    if (ActiveModel != NULL) {
        EModel *B, *N, *A;

        B = A = ActiveModel;
        do {
            N = B->Next;
            delete B;
            B = N;
        } while (B != A);
    }
    ActiveModel = NULL;

    delete SSBuffer;
    SSBuffer = NULL;

    if (ActiveView != NULL) {
        EView *BW, *NW;

        // If EView what is about to be deleted is currently ActiveView, ActiveView moves to next one
        // or if there is no next, it will be set as NULL.
        while ((BW = ActiveView) != NULL) {
            NW = BW->Next;
            delete BW;
        }
        //EView *BW, *NW, *AW;
        //BW = AW = ActiveView;
        //do {
        //    NW = BW->Next;
        //    delete BW;
        //    BW = NW;
        //} while (BW != AW);
    }
    ActiveView = NULL;
}

void EGUI::InterfaceCleanup() {
    while (frames)
        delete frames;
}

void EGUI::Stop() {
    DoSaveHistoryOnExit();

    // free macros
    if (Macros != 0) {
        while (CMacros--) {
            free(Macros[CMacros].Name);

            for (int i = 0; i < Macros[CMacros].Count; ++i)
                if (Macros[CMacros].cmds[i].type == CT_STRING)
                    free(Macros[CMacros].cmds[i].u.string);

            free(Macros[CMacros].cmds);
        }

        free(Macros);

        Macros = 0;
    }

    // free colorizers
    while (EColorize *p = Colorizers) {
        Colorizers = Colorizers->Next;
        delete p;
    }

    // free event maps
    while (EEventMap *em = EventMaps) {
        EventMaps = EventMaps->Next;
        delete em;
    }

    // free modes
    while (EMode *m = Modes) {
        Modes = Modes->fNext;
        delete m;
    }

    // free menus
    if (Menus) {
        int mc, c;

        while (MenuCount--) {
            mc = MenuCount;

            free(Menus[mc].Name);

            while (Menus[mc].Count--) {
                c = Menus[mc].Count;
                free(Menus[mc].Items[c].Name);
            }

            free(Menus[MenuCount].Items);
        }

        free(Menus);

        Menus = NULL;
    }

    // free completion rexexp filter
    extern RxNode *CompletionFilter;
    RxFree(CompletionFilter);

    // free CRegexp array from o_messages.cpp
    FreeCRegexp();
    // free CvsIgnoreRegexp array from o_messages.cpp
    FreeCvsIgnoreRegexp();
    // free SvnIgnoreRegexp array from o_messages.cpp
    FreeSvnIgnoreRegexp();

    // free configuration file path
    free(ConfigSourcePath);
    ConfigSourcePath = NULL;

    EditorCleanup();

    InterfaceCleanup();

    GUI::Stop();
}
