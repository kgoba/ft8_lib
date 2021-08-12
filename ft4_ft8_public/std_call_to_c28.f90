program std_call_to_c28
  
  parameter (NTOKENS=2063592,MAX22=4194304)
  character*6 call_std
  character a1*37,a2*36,a3*10,a4*27
  data a1/' 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ'/
  data a2/'0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ'/
  data a3/'0123456789'/
  data a4/' ABCDEFGHIJKLMNOPQRSTUVWXYZ'/

  nargs=iargc()
  if(nargs.ne.1) then
     print*,'Usage:   std_call_to_c28 <call_std>'
     print*,'Example: std_call_to_c28 K1ABC'
     go to 999
  endif
  call getarg(1,call_std)
  call_std=adjustr(call_std)
  i1=index(a1,call_std(1:1))-1
  i2=index(a2,call_std(2:2))-1
  i3=index(a3,call_std(3:3))-1
  i4=index(a4,call_std(4:4))-1
  i5=index(a4,call_std(5:5))-1
  i6=index(a4,call_std(6:6))-1
  n28=NTOKENS + MAX22 + 36*10*27*27*27*i1 + 10*27*27*27*i2 + &
       27*27*27*i3 + 27*27*i4 + 27*i5 + i6

  write(*,1000) call_std,n28
1000 format('Callsign: ',a6,2x,'c28 as decimal integer:',i10)
  
999 end program std_call_to_c28
