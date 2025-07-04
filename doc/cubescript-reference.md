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
if (~=s $pattern $string) [ echo "Pattern matches" ]

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

// Search in list
findweapon = [
    loop i (listlen $weapons) [
        if (=s (at $weapons $i) $arg1) [
            result $i
        ]
    ]
    result -1
]
pistolindex = (findweapon "pistol")
```

#### String Processing
```cubescript
// String manipulation examples
processname = [
    local name clean
    name = $arg1
    
    // Remove unwanted characters
    clean = (subst $name "_" " ")
    clean = (subst $clean "-" " ")
    
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
        cleargui
    ] [
        if (=s $arg1 "cancel") [
            echo "Cancel button pressed"
            cleargui
        ]
    ]
]
```

### UI Element Creation
```cubescript
// Checkbox with bitwise operations
ui_gameui_variables_typevar = [
    uicheckbox $arg1 (& $$arg2 $arg3) $ui_checksize [
        if (& $$arg2 $arg3) [
            // Uncheck: clear the bit
            $arg2 = (&~ $$arg2 $arg3)
        ] [
            // Check: set the bit
            $arg2 = (| $$arg2 $arg3)
        ]
        echo (concatword "Variable type flags: " $$arg2)
    ]
]

// Dynamic UI generation
createweaponbuttons = [
    weapons = [claw pistol sword shotgun smg flamer plasma zapper rifle corroder]
    loop i (listlen $weapons) [
        weapon = (at $weapons $i)
        uibutton $weapon [
            echo (concatword "Selected weapon: " $weapon)
            selectweapon (getwep $weapon)
        ]
    ]
]
```

### Search and Filtering
```cubescript
// Search functionality
ui_gameui_variables_search = [
    searchstr = (concatword $ui_gameui_variables_searchstr "*")
    numvars = (getvarinfo -1 $ui_gameui_variables_types $ui_gameui_variables_notypes $ui_gameui_variables_flags $ui_gameui_variables_noflags $searchstr 3)
    
    if (> $numvars 0) [
        echo (concatword "Found " $numvars " matching variables")
    ] [
        echo "No variables found matching search criteria"
    ]
]

// Filter by type with bitwise operations
filterVariablesByType = [
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
        if (> $ammo_newweap 0) [
            weapon = $newweap
            echo (concatword "Switched to " (weapname $weapon))
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
    weaponnames = []
    loop i $W_MAX [
        weaponnames = (concat $weaponnames [(weapname $i)])
    ]
]

getcachedweaponname = [
    if (>= $arg1 0) [
        if (< $arg1 (listlen $weaponnames)) [
            result (at $weaponnames $arg1)
        ]
    ]
    result "unknown"
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
