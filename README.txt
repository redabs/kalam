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

* UI
    [ ] i3 style tiling

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

* Tiling
    [ ] Vertical/ horizontal panel splits
    [ ] Direction movement! Not next panel; the right panel, or left, or up, or down!

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

    [ ] Multiple cursors
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

#Discussions:

##Software vs. Hardware rendering
Software pros:
    * Portable
    * Faster and less resource intensive during idle if done right
    * More resistant to dying graphics APIs, OpenGL, Vulkan...
    * No added complexity wrt. graphics hardware that don't apply to us any way.

##Defining cursor movement behavior
When moving the cursor vertically between lines the column position of the cursor changes 
depending on the length of the line it is going to. Standard (what I've observed that most
editors do) behavior is to have two column values; one is where the cursor was before the
vertical movement, call it CursorWas, the other is min(CursorWas, length_of_current_line),
call it CursorIs. E.g. if the cursor moves up one line, it will actually end up on maximum
value of where it was previously and the length of the current line, obviously the cursor
can not go beyond that as it will end up on the same line it started on. 

When the cursor moves horizontally CursorIs is updated to be wherever it ended up, and
CursorWas is to be equal to CursorIs, effectively moving the vertical line that we're 
moving along when moving the cursor vertically. gVim on windows does not allow you to move
the end of the previous line by moving left past the beginning of the current line, sublime
text does allow you to do this. I shall allow it, as it's an easy way to move to the line
above.
