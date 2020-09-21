"Selection first" style editing based on Kakoune.
4coder style project management, i.e. barely any!
i3/vim style tiling with sane navigation.

#TODO: 
    [ ] Self replicating :) (actually use kalam to develop)

* Text
    [ ] Reading font files
        [ ] .ttf
        [ ] .otf
    [ ] Layouting/ wrapping
    [ ] Rendering (software/ hardware?)
        [ ] DPI awareness
        [ ] Ligatures
    [ ] Scaling?
	[ ] Arabic text shaping

* UI
    [x] i3 style tiling
        i3 uses trees with arbtrary number of children per node, we went with a binary tree for simplicity.
    [x] Vertical/ horizontal panel splits
    [x] Direction movement! Not next panel; the right panel, or left, or up, or down!

* Project 
    [ ] Project specification, e.g. project.4coder

* Parsing
    [ ] Tab completion
    [ ] Syntax highlighting
        [ ] C
        [ ] C++
        [ ] Python
        [ ] jai(?)
    [ ] Definitions/ declarations
    [ ] Compiler output (errors)
        [ ] MSVC
        [ ] GCC

* Files
    [ ] Open folder and buffer all source files
    [ ] Switch between open buffers
    [ ] Create/ delete files
    [ ] Rename files
    [ ] Move files? 

* Editing
    [ ] Selections (pattern matching)
        [ ] Inside brackets (), [], {}, <>
        [ ] Inside quotes "", '', ``
        [ ] Next/previous word(s)
        [ ] Paragraph

    [ ] Movement
        [ ] To line X
        [ ] Up/ down N lines
        [ ] To declaration

    [ ] Copy/paste
        "Registers" for multiple cursors? System clipboard?

    [-] Multiple cursors
        Selecting the entire file buffer and narrowing it down to multiple selections of
        a variable and editing all instances of that with multiple cursors is far smoother
        than common search/replace.

    [ ] Undo/redo 
        The Emacs undo/redo system where it forms a tree to keep different revisions
        I found to be super useful. Basically undo goes back one node and does a depth
        first search of all previous node. This way changes are not lost.

    [ ] Searching
        It's extremely important that searching and navigating between search results is 
        smooth. In Kakoune I would rather search forward on the same line than move the 
        cursor by characters or words. When the search is slick as butter to initiate and
        interact with I go from spamming the arrow keys to teleporting to where I want to 
        be.

* Customization
    Initially, none! This is the editor that _I_ want, based on the editors _I_ like. Some 
    customization makes sense, but if it becomes too big of a task then I'd rather edit and
    recompile the source than add scripting support.

#Feature ideas:
    [ ] Parse all function declarations and draw a list on the side with the one we're 
        currently inside highlighted. This is how I _actually_ get a sense of where I am
        in a source file.

	[ ] Toggle a mode where shortcuts are displayed for jumping to any visible token in the
		buffer. Think vim mode for browser where interactive elements are given key commands.

#Media
Early implementation of incremental search & replace using select mode
![](media/select_mode.gif)