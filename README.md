<p align="center">

  <img src="docs/media/banner.svg" alt="Fireflies: A Forest Lantern Game">

</p>



<h3 align="center">Carry a lantern through the dark forest, catch fireflies, and escape a vampire and a witch. Built from scratch in C++ and OpenGL</h3>



<p align="center">

  <b>Manar Toufique Chowdhury</b> &nbsp;·&nbsp; <b>Fahmina Zannat Khan Zebin</b><br>

  CSE 402: Computer Graphics &amp; Image Processing Lab · Metropolitan University, Sylhet Bangladesh

</p>



<p align="center">

  <img src="https://img.shields.io/badge/C%2B%2B-17-00599C?style=for-the-badge&logo=cplusplus&logoColor=white">

  <img src="https://img.shields.io/badge/OpenGL-freeglut-5586A4?style=for-the-badge&logo=opengl&logoColor=white">

  <img src="https://img.shields.io/badge/IDE-Code::Blocks-41AD48?style=for-the-badge">

  <img src="https://img.shields.io/badge/license-MIT-lightgrey?style=for-the-badge">

  <img src="https://img.shields.io/github/stars/manartouf/Fireflies-A-Forest-Lantern-Game?style=for-the-badge&color=FFD54F">

</p>



<p align="center">

  <img src="https://readme-typing-svg.demolab.com?font=Verdana&size=22&pause=1000&color=FFD54F&center=true&vCenter=true&width=700&lines=Carry+the+lantern.;Catch+the+fireflies.;Light+the+way+home." alt="Typing animation">

</p>



<p align="center">

  <img src="docs/media/title.gif" alt="The level menu: easy, medium and hard" width="800">

</p>



<p align="center">

  <a href="#about"><b>About</b></a> ·

  <a href="#the-three-levels"><b>Levels</b></a> ·

  <a href="#gameplay"><b>Gameplay</b></a> ·

  <a href="#gallery"><b>Gallery</b></a> ·

  <a href="#controls"><b>Controls</b></a> ·

  <a href="#how-its-built"><b>Code</b></a> ·

  <a href="#running-it"><b>Run it</b></a> ·

  <a href="#authors"><b>Authors</b></a>

</p>



## About



You are lost in a dark forest with only a net and a lantern jar. Catch fireflies and keep them in the jar: the more of them shine, the wider your circle of light grows, until the whole forest glows and you run home. On the harder levels, something is hunting you: a **vampire** in his crypt, and a **witch** who has locked ten children in a cage.



| Levels | Fireflies in the forest | Image files | Developers |

|:-:|:-:|:-:|:-:|

| **3** | **46** | **none**, everything is drawn in code | **2** |



## The three levels



| Level | Mood | Hunter | Goal |

|:--|:--|:--|:--|

| **Easy**: Lost in the Late Night | Cold blue forest night | none | Catch 25 fireflies, then run home |

| **Medium**: Wandering in Twilight | Purple and orange sky, blood moon, bats | A vampire from his crypt | Catch 25 fireflies and collect 5 lair mushrooms, then run home |

| **Hard**: The Witch's Mansion | Murky green storm night, crows, scarecrows | A witch on a broom | Catch 25 fireflies and collect lair mushrooms, then catch the 10 wisps on the cage locks to free 10 children |



## Gameplay



| **Light**<br>Every firefly in the jar widens your circle of light<br>25 fireflies make you immune to the hunter | **Hunters**<br>Jump over them or blast them back 250 px<br>A blast costs 1 lair mushroom (2 on hard) | **The way home**<br>A golden flash, a speed run, a stream crossed on stepping stones, then the cabin |

|:--|:--|:--|



```mermaid

flowchart LR

  A[Menu] --> B[Story slides] --> C[Playing]

  C -->|goal reached| D[Run home]

  C -->|bitten or caught| E[Game over]

  E -->|R| C

  D -->|R or M| A

```



## Gallery



| ![Easy: catching fireflies](docs/media/catch.gif)<br>Easy: catching fireflies | ![Medium: twilight](docs/media/twilight.gif)<br>Medium: fireflies at twilight |

|:-:|:-:|

| ![Lair mushrooms](docs/media/lair.gif)<br>Collecting lair mushrooms | ![The vampire](docs/media/vampire.gif)<br>The vampire attacks |

| ![Mushroom blast](docs/media/blast.gif)<br>Mushroom blast | ![The witch's mansion](docs/media/mansion.gif)<br>Hard: the witch's mansion |

| ![Blasting the witch](docs/media/witch-blast.gif)<br>Blasting the witch | ![Freeing the children](docs/media/rescue.gif)<br>Freeing the children |

| ![Caught by the witch](docs/media/caught.gif)<br>Game over: caught | ![Home at last](docs/media/homerun.gif)<br>Home at last |



## Controls



| Key | Action |

|:--|:--|

| **A** / **D** or ← / → | Walk |

| **W** or ↑ | Hop |

| **Space** | Hop and swing the net |

| **Click the traveler** | Hop and swing the net |

| **Click anywhere else** | Swing the net only |

| **E** or **right-click** | Mushroom blast (medium and hard) |

| **R** | Restart the level |

| **M** | Back to the menu |

| **Esc** | Quit |



In the menu: **W** / **S** or ↑ / ↓ to choose, **Enter**, **Space** or click to start.



## How it's built



| Technique | Where |

|:--|:--|

| Parallax scrolling | 4 layers of hills and trees moving at different speeds |

| Additive-blend glow | Fireflies, lantern, moon, mushrooms, windows |

| A hole of light in the darkness | Triangle-strip rings around the lantern that grow with each firefly |

| Procedural scenery | Trees, grass, fog and stars from a hash function, so nothing jumps while scrolling |

| Recursive branches | The dead trees around the crypt and the mansion |

| One 1000 × 700 virtual screen | Scaled to any window size with `gluOrtho2D` |



## Project layout



```

Fireflies-A-Forest-Lantern-Game/

├── main.cpp                   the whole game

├── Forest Lantern Game.cbp    Code::Blocks project (Debug and Release targets)

├── deps/freeglut/             freeglut headers, libraries and DLL for Windows builds

├── docs/media/                banner and GIFs

├── CITATION.cff               how to cite this project

├── LICENSE

└── README.md

```



## Running it



1. Download the repository as a ZIP (green **Code** button → **Download ZIP**) and unzip it, or clone it:

   ```

   git clone https://github.com/manartouf/Fireflies-A-Forest-Lantern-Game.git

   ```

2. Install [Code::Blocks](https://www.codeblocks.org/downloads/) (the installer with **mingw** in its name).

3. Open **`Forest Lantern Game.cbp`**. If it asks which compiler to use, pick **GNU GCC Compiler**.

4. Press **F9** to build and run.



The freeglut headers, libraries and DLL are already in the repository under `deps\`, so nothing else needs to be installed.



| Problem | Fix |

|---|---|

| `GL/glut.h: No such file or directory` | the `deps` folder is missing, so unzip the whole repository again |

| "Can't find compiler executable" | install the Code::Blocks version with "mingw" in its name |

| `libfreeglut.dll was not found` when the game starts | build once with F9 (it copies the DLL by itself), or copy `deps\freeglut\bin\x64\libfreeglut.dll` into `bin\Debug` |



## Authors



| **Manar Toufique Chowdhury** | **Fahmina Zannat Khan Zebin** |

|:-:|:-:|



Made for CSE 402 (Computer Graphics &amp; Image Processing Lab) at Metropolitan University, Sylhet Bangladesh. Released under the MIT License. Uses [freeglut](https://freeglut.sourceforge.net/) for the window and input (its license is in `deps/freeglut/COPYING`).

