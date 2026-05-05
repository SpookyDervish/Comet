; ModuleID = 'main'
source_filename = "main"

%Vector = type { i32, i32, i32 }

@str = private constant [12 x i8] c"add lol %x\0A\00"
@str.1 = private constant [51 x i8] c"myVector.x = %d, myVector.y = %d, myVector.z = %d\0A\00"

declare i32 @printf(ptr, ...)

define void @Vector_add(ptr %0, %Vector %1) {
Vector_add_entry:
  %other = alloca ptr, align 8
  store ptr %0, ptr %other, align 8
  %self = load %Vector, ptr %0, align 1
  %print = call i32 (ptr, ...) @printf(ptr @str, %Vector %self)
  ret void
}

define void @Vector_CONSTRUCTOR(ptr %0, i32 %1, i32 %2, i32 %3) {
Vector_CONSTRUCTOR_entry:
  %x = alloca i32, align 4
  store i32 %1, ptr %x, align 4
  %y = alloca i32, align 4
  store i32 %2, ptr %y, align 4
  %z = alloca i32, align 4
  store i32 %3, ptr %z, align 4
  %x1 = load i32, ptr %x, align 4
  %xFieldPtr = getelementptr %Vector, ptr %0, i32 0, i32 0
  store i32 %x1, ptr %xFieldPtr, align 4
  %y2 = load i32, ptr %y, align 4
  %yFieldPtr = getelementptr %Vector, ptr %0, i32 0, i32 1
  store i32 %y2, ptr %yFieldPtr, align 4
  %z3 = load i32, ptr %z, align 4
  %zFieldPtr = getelementptr %Vector, ptr %0, i32 0, i32 2
  store i32 %z3, ptr %zFieldPtr, align 4
  ret void
}

define i32 @main() {
main_entry:
  %myVector = alloca %Vector, align 8
  %selfTmp = alloca %Vector, align 8
  call void @Vector_CONSTRUCTOR(ptr %selfTmp, i32 1, i32 2, i32 3)
  %selfVal = load %Vector, ptr %selfTmp, align 1
  store %Vector %selfVal, ptr %myVector, align 1
  %myVector1 = load %Vector, ptr %myVector, align 1
  %Vector_access = getelementptr %Vector, ptr %myVector, i32 0, i32 0
  %x_field = load i32, ptr %Vector_access, align 4
  %myVector2 = load %Vector, ptr %myVector, align 1
  %Vector_access3 = getelementptr %Vector, ptr %myVector, i32 0, i32 1
  %y_field = load i32, ptr %Vector_access3, align 4
  %myVector4 = load %Vector, ptr %myVector, align 1
  %Vector_access5 = getelementptr %Vector, ptr %myVector, i32 0, i32 2
  %z_field = load i32, ptr %Vector_access5, align 4
  %print = call i32 (ptr, ...) @printf(ptr @str.1, i32 %x_field, i32 %y_field, i32 %z_field)
  %myVector6 = load %Vector, ptr %myVector, align 1
  %selfTmp7 = alloca %Vector, align 8
  call void @Vector_CONSTRUCTOR(ptr %selfTmp7, i32 1, i32 2, i32 3)
  %selfVal8 = load %Vector, ptr %selfTmp7, align 1
  call void @Vector_add(ptr %myVector, %Vector %selfVal8)
  %myVector9 = load %Vector, ptr %myVector, align 1
  %Vector_access10 = getelementptr %Vector, ptr %myVector, i32 0, i32 1
  %y_field11 = load i32, ptr %Vector_access10, align 4
  %0 = icmp ne i32 %y_field11, 4
  %sext = sext i1 %0 to i32
  ret i32 %sext
}
