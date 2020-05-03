"Selection first" style editing based on Kakoune.
4coder style project management, i.e. barely any!
i3 style tiling with sane navigation.

#TODO: 
	[ ] Self-hosting (actually use kalam to develop)

* Text
	[ ] Reading font files
		[ ] .ttf
		[ ] .otf
	[ ] Layouting/ wrapping
	[ ] Rendering (software/ hardware?)
		Software rendering would be the most portable because on any platform we just need a 
		framebuffer.
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
	[ ] Definitions/ declaration
		Jump to definition

	[ ] Compiler output
		[ ] MSVC
		[ ] GCC

* Files
	[ ] Open folder and buffer all source files
	[ ] Switch between open buffers
	[ ] Create/ delete files
	[ ] Rename files
	[ ] Move files? ehh...

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
		interact with I go from spamming the arrow keys to teleporting to where I want to be.

* Customization
	Initially, none! This is the editor that _I_ want, based on the editors _I_ like. Some 
	customization makes sense, but if it becomes too big of a task then I'd rather edit and
	recompile the source than add scripting support.

#Feature ideas:
	[ ] Parse all function declarations and draw a list on the side with the one we're 
		currently inside highlighted. This is how I _actually_ get a sense of where I am
		in a source file.

#What:
The improvement that Kakoune makes over Vim is that it makes editing interactive. You can
always tell what areas are affected by changes. Instead of doing s/foo/bar/g and praying 
you can actually see that you've selected foo and the renaming is done interactively with
multiple cursors. This is something I really want to keep.


#Code Style:
* Variables are PascalCase
* Types are snake_case_with_trailing_underscore_t
* Functions are snake_case
* Macros are CAPITALIZED_SNAKE_CASE

struct my_type_t {
	u32 MyMember;
};

u32
my_function(my_type_t MyArgument) {
	ASSERT_TRUE(MyArgument.MyMember != 0);
	return 8 / MyArgument.MyMember;
}
