
panel_t *
panel_alloc(panel_ctx_t *PanelCtx) {
    if(PanelCtx->FreeList) {
        panel_t *Result = PanelCtx->FreeList;
        PanelCtx->FreeList = PanelCtx->FreeList->Next;
        mem_zero_struct(Result);
        return Result;
    }
    return 0;
}

void
panel_free(panel_ctx_t *PanelCtx, panel_t *Panel) {
    if(!Panel) { return; }
    Panel->Next = PanelCtx->FreeList;
    PanelCtx->FreeList = Panel;
}

void
panel_unlink_leaf_node(panel_ctx_t *PanelCtx, panel_t *Panel) {
    if(PanelCtx->LeafNodeRoot == Panel) { 
        PanelCtx->LeafNodeRoot = Panel->Next;
    }
    
    if(Panel->Prev) {
        Panel->Prev->Next = Panel->Next;
    }
    if(Panel->Next) {
        Panel->Next->Prev = Panel->Prev;
    }
    Panel->Next = Panel->Prev = 0;
}

void
panel_insert_leaf_node(panel_ctx_t *PanelCtx, panel_t *Panel) {
    Panel->Prev = 0;
    Panel->Next = PanelCtx->LeafNodeRoot;
    PanelCtx->LeafNodeRoot = Panel;
    if(Panel->Next) {
        Panel->Next->Prev = Panel;
    }
}

selection_group_t *
add_selection_group(buffer_t *Buf, panel_t *Owner) {
    selection_group_t *SelGrp = sb_add(Buf->SelectionGroups, 1);
    mem_zero_struct(SelGrp);
    SelGrp->Owner = Owner;
    
    return SelGrp;
}

u8
panel_child_index(panel_t *Panel) {
    u8 i = (Panel->Parent->Children[0] == Panel) ? 0 : 1; 
    return i;
}

void
panel_create(panel_ctx_t *PanelCtx) {
    // no hay nada mas
    if(!PanelCtx->FreeList) {
        return;
    }
    
    if(!PanelCtx->Root) {
        panel_t *p = panel_alloc(PanelCtx);;
        p->Buffer = &Ctx.Buffers[0];
        add_selection(p);
        
        selection_group_t *SelGrp = add_selection_group(p->Buffer, p);
        sb_push(SelGrp->Selections, (selection_t) {.Idx = SelGrp->SelectionIdxTop++});
        
        PanelCtx->Root = p;
        PanelCtx->Selected = p;
        
        panel_insert_leaf_node(PanelCtx, p);
        
    } else {
        // We allocate a new sibling and a parent. The selected node, the one which is split, becomes the second sibling of the new parent.
        panel_t *Parent = panel_alloc(PanelCtx);
        panel_t *Sibling = panel_alloc(PanelCtx);
        panel_t *Selected = PanelCtx->Selected;
        
        if(!Sibling || !Parent) { 
            panel_free(PanelCtx, Parent); 
            panel_free(PanelCtx, Sibling); 
            return;
        }
        
        if(PanelCtx->Root == Selected) {
            PanelCtx->Root = Parent;
        }
        
        Parent->Split = Selected->Split;
        Parent->Children[0] = Selected;
        Parent->Children[1] = Sibling;
        
        if(Selected->Parent) {
            Selected->Parent->Children[panel_child_index(Selected)] = Parent;
            Parent->Parent = Selected->Parent;
        }
        
        Selected->Parent = Sibling->Parent = Parent;
        
        Sibling->Split = Selected->Split;
        Sibling->Buffer = Selected->Buffer,
        Sibling->Mode = MODE_Normal;
        Sibling->ScrollX = Selected->ScrollX;
        Sibling->ScrollY = Selected->ScrollY;
        add_selection(Sibling);
        
        {
            selection_group_t *SelGrp = add_selection_group(Sibling->Buffer, Sibling);
            sb_push(SelGrp->Selections, (selection_t) {.Idx = SelGrp->SelectionIdxTop++});
        }
        
        panel_insert_leaf_node(PanelCtx, Sibling);
        
        PanelCtx->Selected = Sibling;
        Sibling->Parent->LastSelected = panel_child_index(Sibling);
    }
}

void
panel_kill(panel_ctx_t *PanelCtx, panel_t *Panel) {
    // Don't kill the root panel or any non-leaf node panel.
    if(Panel == PanelCtx->Root || Panel->Children[0] != 0 || Panel->Children[1] != 0) {
        return;
    }
    
    panel_t *Parent = Panel->Parent;
    panel_t *Sibling = Parent->Children[panel_child_index(Panel) ^ 1];
    
    // Replace Panel->Parent with Panel's sibling.
    if(Parent->Parent) {
        Parent->Parent->Children[panel_child_index(Parent)] = Sibling;
    } 
    
    if(Panel->Parent == PanelCtx->Root) {
        PanelCtx->Root = Sibling;
        Sibling->Parent = 0;
    } else {
        Sibling->Parent = Parent->Parent;
    }
    
    // The panel selection is now transferred over to either the sibling, if it is a leaf node (Sibling->Buffer != 0), or one of its descendants.
    // We decide which one of the decendants to transfer selection to by traversing the tree downwards following the node which was last selected.
    if(Sibling->Buffer) {
        PanelCtx->Selected = Sibling;
    } else {
        panel_t *p = Sibling;
        while(!p->Buffer) {
            p = p->Children[p->LastSelected];
        }
        PanelCtx->Selected = p;
    }
    
    panel_unlink_leaf_node(PanelCtx, Panel);
    
    // TODO: Delete all selection groups owned by Panel.
    
    panel_free(PanelCtx, Panel);
    panel_free(PanelCtx, Parent);
    
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
    u8 n = (Dir == LEFT || Dir == UP) ? 1 : 0;
    while(p != PanelCtx->Root) {
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
