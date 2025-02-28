# Installation design

## Solution screen

After calculating the changes to be performed, APT needs to present the solution to the
user.

### General layout

The screen is organized as a series of blocks, followed by warnings and notices
and finally a prompt.

    BLOCK

    BLOCK

    W: A warning
    Continue? [Y/n]

Each block is separated by an empty line, and follows one of two schemes:

1. Title, Details, optional notes

   This block should have a short title describing what is being shown such as
   "Installing" or "Summary", and one or more line of details, indented by two
   spaces. An optional final line may contain an additional note:

        Title:
          Detail
          Detail
        Notes

    The notes element should be used sparingly, for example, in the autoremove note:

        No longer needed:
           package1 package3
           package2 package4
        Use 'apt autoremove' to remove no longer needed packages.

    The note should be a single sentence, with complete punctuation.


2. Text blobs are short paragraphs that consist only of informational text.
   They may appear before or after the changes lists.
   An example would be the "news" inserted by the Ubuntu Pro hooks.
   Text blobs should be two-three lines long, and have block style formatting,
   with a line length of no more than 79 characters, for example:

        This is an informational blob with no information that appears before
        the list of changes. It contains a lot of text saying stuff.

        Installing:
            package

        Installing dependencies:
            dependency

        This is an important blob with important information that appears
        after the list of changes. It contains a lot of text saying stuff.

        Summary:
          Upgrading: 0, Installing: 23, Removing: 0, Not Upgrading: 1
          Download size: 6,947 kB
          Space needed: 44.7 MB / 16.8 GB available

        Continue? [Y/n]

   They should usually not use any formatting inside the text.

### Order of blocks; standard set of blocks

Generally speaking blocks, should be ordered by increasing importance, and
context relevance. For example, while Suggested packages are less important
then the packages being installed, they are Suggested *by* the packages being
installed, so printing them first is awkward.

The standard blocks and their order are:

1. 'No longer needed' - list of packages that can be autoremoved, and a note how to do so
1. 'Upgrading' - list of packages being upgraded
1. 'Installing' - a list of packages being installed manually
1. 'Installing dependencies' - a list of packages being installed automatically
1. 'Recommended packages' - a list of Recommends that did not get installed
1. 'Suggested packages' - a list of Suggests that did not get installed (Suggests are not installed by default)
1. 'Not upgrading yet' - a list of packages not yet upgraded due to phasing
1. 'Not upgrading' - a list of packages not being upgraded due to dependency issues
1. 'DOWNGRADING' - a list of packages being downgraded, with emphasis as it is unsupported.
1. 'REMOVING' - a list of packages being removed, with emphasis as removals are dangerous
1. 'REMOVING Essential packages:' - a list of Essential packages being removed and a note that it is dangerous.
1. 'Summary' - contains a summary of the changes to be performed

Note that blocks that describe an action are given as a progressive verb, whereas non-action
blocks have a non-verb title.

### Package list layouts

Package lists can be rendered in one of three formats:

1. The standard format is a columnar view following ls(1), top to bottom, left to right,
   showing only the package names.
   If the packages fit in a single line, they are rendered as such.

   Example of multiple lines:

        package1         shortname3 otherpackage5
        longpackagename2 short4     yetanotherpackage6

   Example for a single line

        package1 longpackagename2 shortname3

2. The `-V` format presents one package per line with additional version information,
   in one of the following forms:

        name (version)
        name (old version => new version)

3. The classic layout ("wall of text") is a left to right, top to bottom list that
   wraps around, with package names separated by spaces:

        package1 longpackagename2 shortname3
        short4 otherpackage5 yetanotherpackage6

   This is available in the `--no-list-columns` option.

### Colors and Highlighting of package lists
A solution is essentially a diff to be applied to your system, so we highlight packages
being added as green, and packages being removed as red.
There are a couple more cases of changes calculated, though:

Packages that are being suggested: They are not installed by default, but you can install
them to enhance the functionality of the packages being installed.
We do not want to specifically highlight those, as it's informational only.
What may be interesting is visually distinguishing Suggests that are not even available
in your configured sources, as that is allowed (e.g. main packages Suggest multiverse,
but multiverse is disabled).

Recommends that could not be installed: They are similar to Suggests, but normally
installed by default, so seeing this section is a bit unexpected. We do not believe
they warrant further highlighting, as the section appearing is more than enough.

Upgrades do not change the set of packages installed, but merely their versions, so from
the "present the solution as a diff" approach, it is awkward to present them as green. However,
green is also associated with "good" and upgrades are a normal thing for packages to do,
so highlighting them green is not entirely wrong.

Not highlighting upgrades would make them look similar to non-change lists, like Suggests
and Recommends that failed to install, which would be confusing to the user because it is
making *some* change.

Downgrades are the opposite of upgrades, but importantly they are *unsupported*, we do
not ever test them. It makes sense to highlight their unsupportedness, hence we mark
them yellow.

#### Emphasis in the absence of colors and styles
The headings for removals and downgrades are in upper case to emphasise their
danger.

### The solution summary

The summary contains a line with package change counts per category, followed
by the download size, following by any space changes.


    Summary:
      Upgrading: 0, Installing: 23, Removing: 0, Not Upgrading: 1
      Download size: 6,947 kB
      Space needed: 44.7 MB / 16.8 GB available

**Space changes** are listed as one of the following:

1. The space needed is known, but we can't figure out available space:

       Space needed: 44.7 MB

2. Space needed and available space in the /usr partition

       Space needed: 44.7 MB / 16.8 GB available

3. Space needed and available space, with kernels being installed and a separate /boot:

       Space needed: 44.7 MB / 16.8 GB available
       └─ in /boot:  110 MB / 533 MB available

4. Freed space inverts the order of words vs "Space needed" to make the difference more striking:

       Freed space: 44.7 MB

### Prompting

The final prompt asks the user if they want to continue by prompting either

    Continue? [Y/n]

with the default being 'yes' (as indicated by the upper case character), or

    Continue anyway? [y/N]

with a default of 'no', for example, in the case warnings were shown.
