# Lambda-Photon
Coding Language based on Lambda Calculus focused on optimization
Early Release expect bugs, variables are not mutable

## Syntax

### Variables
```
let x = 42;
let y: i64 = 100;
let pi: f64 = 3.14159;
let msg: str = "Hello";
```

### Types
| Type | Description |
|------|-------------|
| `i8`, `i16`, `i32`, `i64` | Signed integers |
| `u8`, `u16`, `u32`, `u64` | Unsigned integers |
| `f32`, `f64` | Floating point |
| `str` | String |
| `ptr` | Pointer |
| `void` | No value |

### Functions
```
fn add(a: i64, b: i64) -> i64 {
    a + b
};

fn greet(name: str) -> void {
    @print(name);
};
```

### Control Flow
```
// If expression
if x > 0 {
    @print(x);
} else {
    @print(0);
};

// For loop
for i in 0..10 {
    @print(i);
};

// Parallel for loop
@parallel for i in 0..1000 {
    let sq = i * i;
};
```

### Operators
```
// Arithmetic
+  -  *  /  %

// Comparison
==  !=  <  >  <=  >=

// Logical
&&  ||  !
```

### Builtins
```
@print(value);    // Print to stdout
```

### Comments
```
// Single line comment
```

### Usage
```
./photon hello.  lp -o hello
./hello
```

