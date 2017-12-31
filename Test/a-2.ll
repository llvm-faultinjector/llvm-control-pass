; ModuleID = 'a.cpp'
source_filename = "a.cpp"
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@.str = private unnamed_addr constant [4 x i8] c"xxx\00", section "llvm.metadata"
@.str.1 = private unnamed_addr constant [6 x i8] c"a.cpp\00", section "llvm.metadata"

; Function Attrs: noinline nounwind optnone uwtable
define i32 @_Z4foosi(i32 %k) #0 {
entry:
  %k.addr = alloca i32, align 4
  store i32 %k, i32* %k.addr, align 4
  %0 = load i32, i32* %k.addr, align 4
  %add = add nsw i32 10, %0
  ret i32 %add
}

; Function Attrs: noinline nounwind optnone uwtable
define i32 @_Z5foo_cii(i32 %a, i32 %r) #0 {
entry:
  %a.addr = alloca i32, align 4
  %r.addr = alloca i32, align 4
  store i32 %a, i32* %a.addr, align 4
  store i32 %r, i32* %r.addr, align 4
  %0 = load i32, i32* %a.addr, align 4
  %1 = load i32, i32* %r.addr, align 4
  %add = add nsw i32 %0, %1
  %add1 = add nsw i32 %add, 10
  ret i32 %add1
}

; Function Attrs: noinline nounwind optnone uwtable
define i32 @_Z10foo_calledi(i32 %r) #0 {
entry:
  %r.addr = alloca i32, align 4
  %a = alloca i32, align 4
  store i32 %r, i32* %r.addr, align 4
  store i32 1, i32* %a, align 4
  %0 = load i32, i32* %a, align 4
  %1 = load i32, i32* %r.addr, align 4
  %call = call i32 @_Z5foo_cii(i32 %0, i32 %1)
  %add = add nsw i32 %call, 1
  ret i32 %add
}

; Function Attrs: noinline nounwind optnone uwtable
define i32 @_Z3fooi(i32 %k) #0 {
entry:
  %k.addr = alloca i32, align 4
  %a = alloca i32, align 4
  %y = alloca i32, align 4
  %s = alloca i32, align 4
  store i32 %k, i32* %k.addr, align 4
  %a1 = bitcast i32* %a to i8*
  call void @llvm.var.annotation(i8* %a1, i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str, i32 0, i32 0), i8* getelementptr inbounds ([6 x i8], [6 x i8]* @.str.1, i32 0, i32 0), i32 46)
  store i32 5, i32* %y, align 4
  store i32 10, i32* %s, align 4
  %0 = load i32, i32* %y, align 4
  %call = call i32 @_Z10foo_calledi(i32 %0)
  store i32 %call, i32* %a, align 4
  ret i32 0
}

; Function Attrs: nounwind
declare void @llvm.var.annotation(i8*, i8*, i8*, i32) #1

attributes #0 = { noinline nounwind optnone uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { nounwind }

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{!"clang version 5.0.1 (tags/RELEASE_501/final)"}
