This guide is a basic introduction to the development model for Red Eclipse, including the procedure for submitting modifications and creating new ideas. It is suggested you read this in its entirety before starting any development related work, as it contains vital information regarding proper etiquette.

## Preliminaries

Red Eclipse development is done with GIT. From there, you are able to create Issues or Pull Requests (these link to the "base" module, there are separate modules for game data and "site"). If you are unfamiliar with GIT it is recommended to read a few tutorials on the net in order to avoid asking embarrassing questions. GitHub brings the advantage of a familiar interface with a robust set of features aimed at improving collaborative development and built to handle most of what users need from it, including help documentation.

The tree is split up into the following:

* base: https://github.com/red-eclipse/base - The base environment, including everything needed to build the game.
* site: https://github.com/red-eclipse/site - The entirety of the website, minus runtime config and data.

If you are on Windows and don't like the official GitHub application you can try MSysGit or TortoiseGit. If you are on Linux you should be able to obtain the git command line tools from your distribution/package manager.

Here's a rundown of how to setup a new repository:

 git clone --recurse-submodules https://github.com/red-eclipse/base.git redeclipse

## Content/Idea/Proposal Guidelines

* Create a piece of content, come up with an idea, or decide to propose a change you want included in Red Eclipse.
* Then, create a discussion for it at http://redeclipse.net/discuss
* Give the community a chance to provide feedback (minimum time should be about 2 weeks).
* When you feel you have come up with a finished work, idea, or proposal, submit it to the appropriate git module as a pull request.
* Provide a good description along with a link to your original forum topic.
* Attach, or provide direct links to, any files you are attempting to have included here.
* Feel free to share a link to your post while we review it.

## Issue Reporting Guidelines

* Find an issue in the Red Eclipse game and/or engine, verify that you can reproduce the problem and provide steps on how to do so.
* Check on the forums or issue tracker to see if your issue has already been reported and/or resolved upstream.
* When you have verified that the issue still exists and needs to be fixed, submit it to the appropriate git module as an issue or pull request.
* Provide a good description along with a link to your original forum topic (if applicable).
* Attach, or provide direct links to, any files which demonstrates your issue and any patches you might have here.

## General Guidelines

* Only submit finalised works (no work-in-progress items please). If you are just looking for general feedback, you can use the forums (or a git fork!).
* You acknowledge we will probably modify your content before including it (to fix any problems or improve it).
* If you want to modify it at a later point, changes you make will need to be based on our version, otherwise we may not include your updates.
* You must provide an explicit, legally enforceable license. Do not use "public domain", or "no commercial use" / "no modification allowed" clauses. If you don't, it will be assumed you are happy to use the Red Eclipse general license which best applies to the work.

If you are unsure about how to license your work, Creative Commons provides a handy selection tool for their licenses at http://creativecommons.org/choose/ and Red Eclipse content is generally licensed "CC by-sa" (Attribution-ShareAlike), unless otherwise specified: http://creativecommons.org/licenses/by-sa/3.0/

## Rules

* **Behave**: You are a guest in our community.
* **Patience**: Don't expect everyone to give you what you want immediately.
* **Thoughtful**: Try to answer your own questions before asking us.
* **On Topic**: Ask questions related to Red Eclipse only, we won't help you create a game.
* **Speak English**: One language unifies us, multiple languages creates chaos.
* **No Slander**: Do NOT engage in blatant racism, political or religious debates, or engage in slander over the sex and/or orientation of anyone else.

### A Note on Commit Messages

As of yet, there are no strict rules for formatting of commit messages. However, it is good practice to keep commit messages clean and simple while providing a relatively clear idea of what the commit entails.
