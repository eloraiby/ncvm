// bootstrap.4th
: dup   vs.dup  ;
: drop  vs.drop ;
: +     u32.add ;
: -     u32.sub ;
: *     u32.mul ;
: /     u32.div ;
: %     u32.mod ;
: &     u32.and ;
: |     u32.or  ;
: ^     u32.xor ;
: ~     u32.not ;
: >>    u32.shr ;
: <<    u32.shl ;
: =     u32.eq  ;
: !=    u32.neq ;
: >=    u32.geq ;
: <=    u32.leq ;
: >     u32.gt  ;
: <     u32.lt  ;
: ?     cond    ;
: ##    call    ;

: >l    ls.push ;
: l@    ls.read ;

: test+ 1 2 + .i ;
: test* 2 3 * .i ;

: test-lambda-false 0 { 11 } { 22 } ? .i ;
: test-lambda-true  1 { 11 } { 22 } ? .i ;

: test-call-return  { 13 } ## .i ;
: test-call-tail-intern { 13 } ## ;
: test-call-tail    1 2 test-call-tail-intern .i .i .i ;

: test-locals
    1 >l
    2 >l
    3 >l

    0 l@
    1 l@
    *
    2 l@
    +
    .i
    ;

: test-rec
    dup
    dup .i
    0 =
    { 32123 .i }
    { 1 -
      test-rec }
    ?
    ;

: test-r2 100 test-rec ;

