
panel_t *
panel_alloc(panel_ctx_t *PanelCtx) {
    if(PanelCtx->FreeList) {
        panel_t *Result = &PanelCtx->FreeList->Panel;
        PanelCtx->FreeList = PanelCtx->FreeList->Next;
        mem_zero_struct(Result);
        return Result;
    }
    return 0;
}

void
panel_free(panel_ctx_t *PanelCtx, panel_t *Panel) {
    if(!Panel) { return; }
    panel_free_node_t *n = (panel_free_node_t *)Panel;
    n->Next = PanelCtx->FreeList;
    PanelCtx->FreeList = n;
}

void
panel_create(panel_ctx_t *PanelCtx) {
    // no hay nada mas
    if(!PanelCtx->FreeList) {
        return;
    }
    
    if(!PanelCtx->Root) {
        PanelCtx->Root = panel_alloc(PanelCtx);
        PanelCtx->Root->Buffer = &Ctx.Buffers[0];
        PanelCtx->Selected = PanelCtx->Root;
        
        add_selection(PanelCtx->Root);
    } else {
        panel_t *Parent = PanelCtx->Selected;
        panel_t *Left = panel_alloc(PanelCtx);
        panel_t *Right = panel_alloc(PanelCtx);
        
        if(!Right || !Left) { 
            panel_free(PanelCtx, Left); 
            panel_free(PanelCtx, Right); 
            return;
        }
        
        *Left = *Parent;
        Parent->Children[0] = Left;
        Parent->Children[1] = Right;
        Left->Parent = Right->Parent = Parent;
        
        Right->Split = Left->Split;
        Right->Buffer = Left->Buffer,
        Right->Mode = MODE_Normal;
        add_selection(Right);
        
        Parent->Buffer = 0;
        
        PanelCtx->Selected = Right;
        Right->Parent->LastSelected = 1;
    }
}

s32
panel_child_index(panel_t *Panel) {
    s32 i = (Panel->Parent->Children[0] == Panel) ? 0 : 1; 
    return i;
}

void
panel_kill(panel_ctx_t *PanelCtx, panel_t *Panel) {
    if(!PanelCtx->Root->Children[0] && !PanelCtx->Root->Children[1]) { return; }
    
    if(Panel->Parent) {
        s32 Idx = panel_child_index(Panel);
        panel_t *Sibling = Panel->Parent->Children[Idx ^ 1];
        
        // Copy sibling to parent as panels are leaf nodes so parent can't have only one valid child.
        Sibling->Parent = Panel->Parent->Parent;
        if(Sibling->Children[0]) {
            Sibling->Children[0]->Parent = Panel->Parent;
        }
        if(Sibling->Children[1]) {
            Sibling->Children[1]->Parent = Panel->Parent;
        }
        
        *Panel->Parent = *Sibling;
        
        panel_t *p = Panel->Parent;
        while(!p->Buffer) {
            p = p->Children[p->LastSelected];
        }
        PanelCtx->Selected = p;
        
        panel_free(PanelCtx, Sibling);
        panel_free(PanelCtx, Panel);
        sb_free(Panel->Selections);
    } else {
        PanelCtx->Selected = Panel->Parent;
        panel_free(PanelCtx, Panel);
        PanelCtx->Root = 0;
    }
}

void
panel_move_selection(panel_ctx_t *PanelCtx, dir_t Dir) {
    // Panel->Children[0] is always left or above 
    panel_t *Panel = PanelCtx->Selected;
    if(PanelCtx->Root == Panel) {
        return;
    }
    
    s32 Idx = 0;
    panel_t *p = Panel;
    b32 Found = false;
    split_mode_t SplitMode = (Dir == LEFT || Dir == RIGHT) ? SPLIT_Vertical : SPLIT_Horizontal;
    while(p != PanelCtx->Root) {
        u8 n = (Dir == LEFT || Dir == UP) ? 1 : 0;
        Idx = panel_child_index(p);
        if(Idx == n && p->Parent->Split == SplitMode) {
            p = p->Parent->Children[n ^ 1];
            p->Parent->LastSelected = n ^ 1;
            Found = true;
            break;
        }
        p = p->Parent;
    }
    
    if(Found) {
        while(!p->Buffer) {
            p = p->Children[p->LastSelected];
        }
        
        panel_t *Par = p->Parent;
        // Only move to the last selected node if both children are leaves (i.e. they have buffers attached)
        if(Par != Panel->Parent && Par->Children[0]->Buffer && Par->Children[1]->Buffer) {
            PanelCtx->Selected = p->Parent->Children[p->Parent->LastSelected];
        } else {
            PanelCtx->Selected = p;
            p->Parent->LastSelected = (u8)panel_child_index(p);
        }
    }
}
