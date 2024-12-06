# if

```makefile
$(if condition, then-part, else-part)
```
makefile中的条件判断, 如果 condition 判断非空, 则认为条件成立.

当条件成立时候, 执行 `then-part` 部分，当条件不成立时候则执行 `else-part`, 条件不成立部分可以省略, 默认为空.


