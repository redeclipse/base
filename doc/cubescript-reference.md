# CubeScript Language Reference

This document provides a comprehensive reference for the CubeScript scripting language used in Red Eclipse for configuration, UI, and game logic.

## Syntax Fundamentals

### Basic Syntax Rules
- **Variables**: `$varname` - Access variable value
- **Blocks**: `[code]` - Group statements
- **Arguments**: `$arg1`, `$arg2`, etc. - Function parameters
- **Lists**: Space-separated values
- **Comments**: `//` for single-line comments

### Variable Types and Operations

#### Integer Operations
```cubescript
// Arithmetic operators
result = (+ $a $b)        // Addition
result = (- $a $b)        // Subtraction  
result = (* $a $b)        // Multiplication
result = (div $a $b)      // Division
result = (mod $a $b)      // Modulo

// Comparison operators
if (= $a $b) [ echo "Equal" ]
if (!= $a $b) [ echo "Not equal" ]
if (< $a $b) [ echo "Less than" ]
if (> $a $b) [ echo "Greater than" ]
if (<= $a $b) [ echo "Less or equal" ]
if (>= $a $b) [ echo "Greater or equal" ]

// Bitwise operations
result = (& $a $b)        // Bitwise AND
result = (| $a $b)        // Bitwise OR
result = (^ $a $b)        // Bitwise XOR
result = (~ $a)           // Bitwise NOT
result = (<< $a $shift)   // Left shift
result = (>> $a $shift)   // Right shift
result = (&~ $a $b)       // AND NOT (clear bits)
```

#### Floating Point Operations
```cubescript
// Float arithmetic (note the 'f' suffix)
result = (+f $a $b)       // Float addition
result = (-f $a $b)       // Float subtraction
result = (*f $a $b)       // Float multiplication
result = (divf $a $b)     // Float division
result = (modf $a $b)     // Float modulo

// Float comparisons
if (=f $a $b) [ echo "Float equal" ]
if (!=f $a $b) [ echo "Float not equal" ]
if (<f $a $b) [ echo "Float less than" ]
if (>f $a $b) [ echo "Float greater than" ]
```

#### String Operations
```cubescript
// String comparison
if (=s $str1 $str2) [ echo "Strings equal" ]
if (!=s $str1 $str2) [ echo "Strings not equal" ]
if (~=s $str1 $str2) [ echo "Strings equal, ignoring case" ]

// String manipulation
result = (concatword $str1 $str2 $str3)    // Concatenate strings
result = (substr $string $start $length)   // Extract substring
result = (strlen $string)                  // String length
result = (strstr $haystack $needle)        // Find substring (returns position or -1)
result = (listlen $list)                   // List length
result = (at $list $index)                 // Get list element at index
```

### Control Flow

#### Conditional Statements
```cubescript
// Basic if statement
if (> $health 50) [
    echo "Health is good"
]

// If-else statement
if (> $health 50) [
    echo "Health is good"
] [
    echo "Health is low"
]

// Nested conditions
if (> $health 75) [
    echo "Excellent health"
] [
    if (> $health 25) [
        echo "Fair health"
    ] [
        echo "Critical health"
    ]
]

// Boolean operators
if (&& (> $health 50) (< $health 100)) [
    echo "Health in normal range"
]

if (|| (< $health 25) (> $health 75)) [
    echo "Health needs attention"
]

if (! $gameover) [
    echo "Game continues"
]
```

#### Loops
```cubescript
// Basic loop
loop i 10 [
    echo (concatword "Iteration: " $i)
]

// Loop with variables
count = 5
loop i $count [
    echo (concatword "Count: " $i " of " $count)
]

// Nested loops
loop x 3 [
    loop y 3 [
        echo (concatword "Position: " $x "," $y)
    ]
]
```

### Functions and Aliases

#### Function Definition
```cubescript
// Simple function
greet = [
    echo (concatword "Hello " $arg1)
]
greet "World"

// Function with multiple parameters
addnumbers = [
    result = (+ $arg1 $arg2)
    echo (concatword $arg1 " + " $arg2 " = " $result)
    result $result
]
sum = (addnumbers 5 10)

// Function with validation
safedivide = [
    if (!= $arg2 0) [
        result (divf $arg1 $arg2)
    ] [
        echo "Error: Division by zero"
        result 0
    ]
]

// Complex function example
getrandomweapon = [
    local weapons
    weapons = [pistol rifle shotgun smg flamer plasma]
    result (at $weapons (rnd (listlen $weapons)))
]
weapon = (getrandomweapon)
```

#### Local Variables
```cubescript
// Use 'local' to create function-scoped variables
calculate = [
    local temp result
    temp = (* $arg1 $arg1)
    result = (+ $temp $arg2)
    echo (concatword "Result: " $result)
    result $result
]
```

### Advanced Features

#### List Manipulation
```cubescript
// Create and manipulate lists
weapons = [claw pistol sword shotgun smg]
echo (concatword "Weapon count: " (listlen $weapons))
echo (concatword "First weapon: " (at $weapons 0))
echo (concatword "Last weapon: " (at $weapons -1))

// Add to list
weapons = (concat $weapons [flamer plasma])

// Search in list (listfind returns the index of the first match, or -1)
findweapon = [
    result (listfind w $weapons [=s $w $arg1])
]
pistolindex = (findweapon "pistol")
```

#### String Processing
```cubescript
// String manipulation examples
processname = [
    local name clean
    name = $arg1
    
    // Replace unwanted characters
    clean = (strreplace $name "_" " ")
    clean = (strreplace $clean "-" " ")
    
    // Capitalize first letter
    if (strlen $clean) [
        first = (substr $clean 0 1)
        rest = (substr $clean 1)
        result (concatword (strupper $first) (strlower $rest))
    ] [
        result ""
    ]
]

playername = (processname "john_doe")
```

## UI System Integration

### UI Event Handling
```cubescript
// UI event handlers
ui_gameui_variables_on_open = [
    ui_gameui_variables_index = 0
    ui_gameui_variables_num = -1
    ui_gameui_variables_page = 0
    ui_gameui_variables_reset = 1
    echo "Variables UI opened"
]

ui_gameui_variables_on_close = [
    echo "Variables UI closed"
]

// Button actions
ui_button_action = [
    if (=s $arg1 "ok") [
        echo "OK button pressed"
        hideui $uiname
    ] [
        if (=s $arg1 "cancel") [
            echo "Cancel button pressed"
            hideui $uiname
        ]
    ]
]
```

### UI Element Creation
```cubescript
// Switch toggling one bit of a variable (gameui widgets, config/ui/game/widgets/)
ui_gameui_example_flags = [
    ui_gameui_compactswitch playerauras [
        p_tip = "Glow around other players"
        p_get = [& $arg1 4]
        p_set = [? $arg1 (| $playerauras 4) (&~ $playerauras 4)]
        p_id  = #(gameui_get_id compactswitch)
    ]
]

// Dynamic UI generation (W_NAMES lists weapon names in W_ENUM order, W_MAX is the count)
// '#' definitions expand #(...) once when defined: here it reserves W_MAX widget ids.
// p_on_click runs a frame later, so it is built as a string while the loop variable still exists.
# createweaponbuttons = [
    local _base
    _base = #(gameui_get_id button $W_MAX)

    loop i $W_MAX [
        ui_gameui_button [
            p_label    = (at $W_NAMES $i)
            p_on_click = (concat weapon $i)
            p_id       = (+ $_base $i)
        ]
    ]
]
```

### Search and Filtering
```cubescript
// Search functionality
// getvarinfo <index, -1 for the count> <types> <notypes> <flags> <noflags> <search> <searchtype> [changed only]
ui_gameui_variables_search = [
    searchstr = (concatword "*" $gameui_variables_query "*")
    numvars = (getvarinfo -1 $gameui_variables_types 0 $idfbitpersist 0 $searchstr 3 $gameui_variables_changed)
    
    if (> $numvars 0) [
        echo (concatword "Found " $numvars " matching variables")
    ] [
        echo "No variables found matching search criteria"
    ]
]

// Filter by type with bitwise operations
filtervariablesbytype = [
    types = $arg1  // Bitfield of desired types
    count = 0
    
    // Check each variable type bit
    if (& $types (<< 1 $ididxvar)) [
        echo "Including integer variables"
        count = (+ $count 1)
    ]
    if (& $types (<< 1 $ididxfvar)) [
        echo "Including float variables"
        count = (+ $count 1)
    ]
    if (& $types (<< 1 $ididxsvar)) [
        echo "Including string variables"
        count = (+ $count 1)
    ]
    
    result $count
]
```

## Game Logic Integration

### Player State Management
```cubescript
// Weapon switching logic
switchweapon = [
    newweap = $arg1
    if (&& (>= $newweap 0) (< $newweap $W_MAX)) [
        if (> (ammoclip $newweap) 0) [
            weapon $newweap
            echo (concatword "Switched to " (at $W_NAMES $newweap))
        ] [
            echo "No ammo for that weapon"
        ]
    ] [
        echo "Invalid weapon"
    ]
]
```

## Performance and Best Practices

### Efficient Code Patterns
```cubescript
// Cache frequently used values
initcache = [
    weaponnames = $W_NAMES
]

getcachedweaponname = [
    if (&& (>= $arg1 0) (< $arg1 (listlen $weaponnames))) [
        result (at $weaponnames $arg1)
    ] [
        result "unknown"
    ]
]

// Avoid repeated expensive operations
optimizedloop = [
    maxval = (listlen $items)  // Cache the length
    loop i $maxval [
        item = (at $items $i)  // Cache the item reference
        // Process item...
    ]
]
```

### Error Handling
```cubescript
// Safe operations with validation
safeat = [
    list = $arg1
    index = $arg2
    
    if (&& (>= $index 0) (< $index (listlen $list))) [
        result (at $list $index)
    ] [
        echo (concatword "Index " $index " out of bounds for list of length " (listlen $list))
        result ""
    ]
]

// Input validation
validateinput = [
    value = $arg1
    minval = $arg2
    maxval = $arg3
    
    if (&& (>= $value $minval) (<= $value $maxval)) [
        result 1
    ] [
        echo (concatword "Value " $value " must be between " $minval " and " $maxval)
        result 0
    ]
]
```

### Errors and the script trace
When CubeScript fails, the error is followed by an indented line, the script trace, saying where it happened:

```
Unknown command: nosuchcmd
  in maps_menu on-show > mapsel_refresh > mapsel_row (config/ui/maps.cfg:212)
Unknown command: backgroundmenushift
  at init.cfg:1
Unknown command: nosuchbind
  in bind F9
```

It has up to three parts:

- **Script origin**: what started the code running. `loading <file>` for a file being executed, the UI's name for its
  build, with `on-show`, `on-hide`, `visible test`, `force test` or `on-change` for those actions, or the state
  command that ran it (`uirelease`, `uihover` and so on), `bind <key>` for a key bind, `console` for something typed
  in the console, and `timer, from <origin>` for a `sleep` body, naming whatever scheduled it.
- **Alias chain**: the aliases it was running inside, outermost first. Past `dbgalias` aliases only the outermost and
  the innermost ones are shown, with `..` between them.
- **Position**: the file and line of the statement. Errors while a file loads always have one. Elsewhere it needs
  `dbgscriptlines 1`; without it, the innermost alias a file defined gives where it was defined instead
  (`defined at config/ui/maps.cfg:198`). Aliases and UIs defined while a file loads remember where, so with
  `dbgscriptlines 1` code inside them has a position too, counted from where they were defined. Code built from
  strings while the game runs (`format`, `concat`, a `sleep` or bind body) has none; the nearest code calling it that
  has one is given instead.

Errors printed by commands themselves (`Could not read <file>` and the like), the sandbox's refusals of map script, and
warnings that script causes (a UI command used inside the wrong kind of element, a missing grass shape, a texture
redirect that drops something) get the trace too, except when it would be the same as the one just printed. A shader is
compiled long after the config that defined it ran, so its `GLSL ERROR` names where it was defined instead:
`GLSL ERROR (FS:name), defined at config/glsl/foo.cfg:120`. When an error and its trace repeat, they are printed once, and a second
later the error is printed again with `(repeated N times)`; errors that take turns are each counted. After that it is
printed in full again the next time it happens, so an error that keeps happening shows about once a second. Parse errors such as `Missing "]"` name
the file and line in their trace like any other error, when the text came from a file.

| Variable | Default | Meaning |
|---|---|---|
| `dbgscript` | 1 | 0 prints errors without a trace, 1 on one line, 2 with each alias on a line of its own |
| `dbgalias` | 20 | How many aliases of the chain to show |
| `dbgscriptlines` | 0 | 1 notes the line of each statement as code runs, so errors outside file loading name it; not saved |

`nodebug [ code ]` runs code without printing its errors at all, which suits looking something up that may not exist.

Finding a position costs nothing until an error is printed: the compiled code keeps a table of statement lines that is
read only then. What it does cost is noting, at each command call, alias call and variable write, how far the code got,
about 1.5% more instructions in a menu frame. So that is only done while a file loads and with `dbgscriptlines 1`: when
an error has no line, turn it on and make the error happen again.
