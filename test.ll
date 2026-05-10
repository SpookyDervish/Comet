; ModuleID = 'main'
source_filename = "main"

%Animal = type { i32 }
%Dog = type { i32 }

@str = private constant [5 x i8] c"...\0A\00"
@str.1 = private constant [7 x i8] c"Woof!\0A\00"
@str.2 = private constant [4 x i8] c"%d\0A\00"

declare i32 @printf(ptr, ...)

define void @Animal_speak(ptr %0) {
Animal_speak_entry:
  %print = call i32 (ptr, ...) @printf(ptr @str)
  ret void
}

define void @Animal_CONSTRUCTOR(ptr %0) {
Animal_CONSTRUCTOR_entry:
  %age_default = getelementptr %Animal, ptr %0, i32 0, i32 0
  store i32 0, ptr %age_default, align 4
  ret void
}

define void @Dog_speak(ptr %0) {
Dog_speak_entry:
  %print = call i32 (ptr, ...) @printf(ptr @str.1)
  ret void
}

define void @Dog_CONSTRUCTOR(ptr %0) {
Dog_CONSTRUCTOR_entry:
  %age_default = getelementptr %Dog, ptr %0, i32 0, i32 0
  store i32 0, ptr %age_default, align 4
  ret void
}

define i32 @main() {
main_entry:
  %myAnimal = alloca %Animal, align 8
  %selfTmp = alloca %Animal, align 8
  call void @Animal_CONSTRUCTOR(ptr %selfTmp)
  %selfVal = load %Animal, ptr %selfTmp, align 4
  store %Animal %selfVal, ptr %myAnimal, align 4
  %myAnimal1 = load %Animal, ptr %myAnimal, align 4
  call void @Animal_speak(ptr %myAnimal)
  %myDog = alloca %Dog, align 8
  %selfTmp2 = alloca %Animal, align 8
  call void @Animal_CONSTRUCTOR(ptr %selfTmp2)
  %selfVal3 = load %Animal, ptr %selfTmp2, align 4
  %tmp = alloca %Animal, align 8
  store %Animal %selfVal3, ptr %tmp, align 4
  %0 = load %Dog, ptr %tmp, align 4
  store %Dog %0, ptr %myDog, align 4
  %myDog4 = load %Dog, ptr %myDog, align 4
  call void @Dog_speak(ptr %myDog)
  %myDog5 = load %Dog, ptr %myDog, align 4
  %Dog_access = getelementptr %Dog, ptr %myDog, i32 0, i32 0
  %age_field = load i32, ptr %Dog_access, align 4
  %1 = add i32 %age_field, 1
  %ageFieldPtr = getelementptr %Dog, ptr %myDog, i32 0, i32 0
  store i32 %1, ptr %ageFieldPtr, align 4
  %myDog6 = load %Dog, ptr %myDog, align 4
  %Dog_access7 = getelementptr %Dog, ptr %myDog, i32 0, i32 0
  %age_field8 = load i32, ptr %Dog_access7, align 4
  %print = call i32 (ptr, ...) @printf(ptr @str.2, i32 %age_field8)
  ret i32 0
}
