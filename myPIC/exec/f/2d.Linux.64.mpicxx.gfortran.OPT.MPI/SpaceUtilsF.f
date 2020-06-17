      subroutine BWENOFACE(
     & facePhi
     & ,ifacePhilo0,ifacePhilo1
     & ,ifacePhihi0,ifacePhihi1
     & ,nfacePhicomp
     & ,cellPhi
     & ,icellPhilo0,icellPhilo1
     & ,icellPhihi0,icellPhihi1
     & ,ncellPhicomp
     & ,faceVel
     & ,ifaceVello0,ifaceVello1
     & ,ifaceVelhi0,ifaceVelhi1
     & ,ifaceBoxlo0,ifaceBoxlo1
     & ,ifaceBoxhi0,ifaceBoxhi1
     & ,idir
     & )
      implicit none
      integer*8 ch_flops
      COMMON/ch_timer/ ch_flops
      integer CHF_ID(0:5,0:5)
      data CHF_ID/ 1,0,0,0,0,0 ,0,1,0,0,0,0 ,0,0,1,0,0,0 ,0,0,0,1,0,0 ,0
     &,0,0,0,1,0 ,0,0,0,0,0,1 /
      integer nfacePhicomp
      integer ifacePhilo0,ifacePhilo1
      integer ifacePhihi0,ifacePhihi1
      REAL*8 facePhi(
     & ifacePhilo0:ifacePhihi0,
     & ifacePhilo1:ifacePhihi1,
     & 0:nfacePhicomp-1)
      integer ncellPhicomp
      integer icellPhilo0,icellPhilo1
      integer icellPhihi0,icellPhihi1
      REAL*8 cellPhi(
     & icellPhilo0:icellPhihi0,
     & icellPhilo1:icellPhihi1,
     & 0:ncellPhicomp-1)
      integer ifaceVello0,ifaceVello1
      integer ifaceVelhi0,ifaceVelhi1
      REAL*8 faceVel(
     & ifaceVello0:ifaceVelhi0,
     & ifaceVello1:ifaceVelhi1)
      integer ifaceBoxlo0,ifaceBoxlo1
      integer ifaceBoxhi0,ifaceBoxhi1
      integer idir
      integer n, ncell, i,j
      integer ii,jj
      REAL*8 eps
      REAL*8 fl,fr,bl,br,al,ar,wl,wr
      REAL*8 c1l,c2l,c1r,c2r
      REAL*8 wmax,wmin
      REAL*8 onept5
      ii = CHF_ID(idir, 0)
      jj = CHF_ID(idir, 1)
      onept5 = (1.0d0) + (0.500d0)
      eps = 1.d-6
      do n=0, (nfacePhicomp-1)
         if(nfacePhicomp.lt.ncellPhicomp) then
            ncell = idir
         else
            ncell = n
         endif
      do j = ifaceBoxlo1,ifaceBoxhi1
      do i = ifaceBoxlo0,ifaceBoxhi0
        fl = (1.000d0 / 6.000d0)*(
     & - cellPhi(i-2*ii,j-2*jj,ncell)
     & + (5.0d0)*cellPhi(i-ii,j-jj,ncell)
     & + (2.0d0)*cellPhi(i,j,ncell) )
        fr = (1.000d0 / 6.000d0)*(
     & (2.0d0)*cellPhi(i-ii,j-jj,ncell)
     & + (5.0d0)*cellPhi(i,j,ncell)
     & - cellPhi(i+ii,j+jj,ncell) )
        c1l = ( cellPhi(i,j,ncell)
     & - (2.0d0)*cellPhi(i-ii,j-jj,ncell)
     & + cellPhi(i-2*ii,j-2*jj,ncell) )
        c2l = ( cellPhi(i,j,ncell)
     & - cellPhi(i-2*ii,j-2*jj,ncell) )
        c1r = ( cellPhi(i+ii,j+jj,ncell)
     & - (2.0d0)*cellPhi(i,j,ncell)
     & + cellPhi(i-ii,j-jj,ncell) )
        c2r = ( cellPhi(i+ii,j+jj,ncell)
     & - cellPhi(i-ii,j-jj,ncell) )
        bl = (4.0d0)*(c1l**2)*(1.000d0 / 3.000d0)+(0.500d0)*c1l*c2l+(0.2
     &50d0)*c2l**2
        br = (4.0d0)*(c1r**2)*(1.000d0 / 3.000d0)-(0.500d0)*c1r*c2r+(0.2
     &50d0)*c2r**2
        al = (1.0d0)/((eps+bl)**2)
        ar = (1.0d0)/((eps+br)**2)
        wl = al/(al+ar)
        wr = ar/(al+ar)
        al = wl*((3.0d0)*(0.250d0)+wl*(wl-onept5))
        ar = wr*((3.0d0)*(0.250d0)+wr*(wr-onept5))
        wl = al/(al+ar)
        wr = ar/(al+ar)
        wmax = max(wl,wr)
        wmin = min(wl,wr)
        if( faceVel(i,j).gt.(0.0d0) ) then
          wl = wmax
          wr = wmin
        else
          wl = wmin
          wr = wmax
        end if
        facePhi(i,j,n) = ( wl*fl + wr*fr )
      enddo
      enddo
      enddo
      return
      end
      subroutine WENO5FACE(
     & facePhi
     & ,ifacePhilo0,ifacePhilo1
     & ,ifacePhihi0,ifacePhihi1
     & ,nfacePhicomp
     & ,cellPhi
     & ,icellPhilo0,icellPhilo1
     & ,icellPhihi0,icellPhihi1
     & ,ncellPhicomp
     & ,faceVel
     & ,ifaceVello0,ifaceVello1
     & ,ifaceVelhi0,ifaceVelhi1
     & ,smoothFn
     & ,ismoothFnlo0,ismoothFnlo1
     & ,ismoothFnhi0,ismoothFnhi1
     & ,nsmoothFncomp
     & ,ifaceBoxlo0,ifaceBoxlo1
     & ,ifaceBoxhi0,ifaceBoxhi1
     & ,idir
     & )
      implicit none
      integer*8 ch_flops
      COMMON/ch_timer/ ch_flops
      integer CHF_ID(0:5,0:5)
      data CHF_ID/ 1,0,0,0,0,0 ,0,1,0,0,0,0 ,0,0,1,0,0,0 ,0,0,0,1,0,0 ,0
     &,0,0,0,1,0 ,0,0,0,0,0,1 /
      integer nfacePhicomp
      integer ifacePhilo0,ifacePhilo1
      integer ifacePhihi0,ifacePhihi1
      REAL*8 facePhi(
     & ifacePhilo0:ifacePhihi0,
     & ifacePhilo1:ifacePhihi1,
     & 0:nfacePhicomp-1)
      integer ncellPhicomp
      integer icellPhilo0,icellPhilo1
      integer icellPhihi0,icellPhihi1
      REAL*8 cellPhi(
     & icellPhilo0:icellPhihi0,
     & icellPhilo1:icellPhihi1,
     & 0:ncellPhicomp-1)
      integer ifaceVello0,ifaceVello1
      integer ifaceVelhi0,ifaceVelhi1
      REAL*8 faceVel(
     & ifaceVello0:ifaceVelhi0,
     & ifaceVello1:ifaceVelhi1)
      integer nsmoothFncomp
      integer ismoothFnlo0,ismoothFnlo1
      integer ismoothFnhi0,ismoothFnhi1
      REAL*8 smoothFn(
     & ismoothFnlo0:ismoothFnhi0,
     & ismoothFnlo1:ismoothFnhi1,
     & 0:nsmoothFncomp-1)
      integer ifaceBoxlo0,ifaceBoxlo1
      integer ifaceBoxhi0,ifaceBoxhi1
      integer idir
      integer n, ncell, i,j
      integer ii,jj
      REAL*8 eps
      REAL*8 fl,fr,bl,br,al,ar,wl,wr
      REAL*8 c0,c1,c2,c3
      REAL*8 a0,a1,a2,asuminv
      REAL*8 b0,b1,b2
      REAL*8 v0,v1,v2
      REAL*8 w0,w1,w2
      REAL*8 g
      ii = CHF_ID(idir, 0)
      jj = CHF_ID(idir, 1)
      eps = 1.d-6
      do n=0, (nfacePhicomp-1)
         if(nfacePhicomp.lt.ncellPhicomp) then
            ncell = idir
         else
            ncell = n
         endif
      do j = ifaceBoxlo1,ifaceBoxhi1
      do i = ifaceBoxlo0,ifaceBoxhi0
        if( faceVel(i,j).gt.(0.0d0) ) then
          v0 = (1.000d0 / 6.000d0)*(
     & (2.0d0)*cellPhi(i-ii,j-jj,ncell)
     & + (5.0d0)*cellPhi(i,j,ncell)
     & - cellPhi(i+ii,j+jj,ncell) )
          v1 = (1.000d0 / 6.000d0)*(
     & - cellPhi(i-2*ii,j-2*jj,ncell)
     & + (5.0d0)*cellPhi(i-ii,j-jj,ncell)
     & + (2.0d0)*cellPhi(i,j,ncell) )
          v2 = (1.000d0 / 6.000d0)*(
     & (2.0d0)*cellPhi(i-3*ii,j-3*jj,ncell)
     & - (7.0d0)*cellPhi(i-2*ii,j-2*jj,ncell)
     & + (11.0d0)*cellPhi(i-ii,j-jj,ncell) )
          c0 = cellPhi(i+ii,j+jj,ncell)
     & - cellPhi(i,j,ncell)
          c1 = cellPhi(i,j,ncell)
     & - cellPhi(i-ii,j-jj,ncell)
          c2 = cellPhi(i-ii,j-jj,ncell)
     & - cellPhi(i-2*ii,j-2*jj,ncell)
          c3 = cellPhi(i-2*ii,j-2*jj,ncell)
     & - cellPhi(i-3*ii,j-3*jj,ncell)
        else
          v0 = (1.000d0 / 6.000d0)*(
     & (2.0d0)*cellPhi(i,j,ncell)
     & + (5.0d0)*cellPhi(i-ii,j-jj,ncell)
     & - cellPhi(i-2*ii,j-2*jj,ncell) )
          v1 = (1.000d0 / 6.000d0)*(
     & - cellPhi(i+ii,j+jj,ncell)
     & + (5.0d0)*cellPhi(i,j,ncell)
     & + (2.0d0)*cellPhi(i-ii,j-jj,ncell) )
          v2 = (1.000d0 / 6.000d0)*(
     & (2.0d0)*cellPhi(i+2*ii,j+2*jj,ncell)
     & - (7.0d0)*cellPhi(i+ii,j+jj,ncell)
     & + (11.0d0)*cellPhi(i,j,ncell) )
          c0 = cellPhi(i-2*ii,j-2*jj,ncell)
     & - cellPhi(i-ii,j-jj,ncell)
          c1 = cellPhi(i-ii,j-jj,ncell)
     & - cellPhi(i,j,ncell)
          c2 = cellPhi(i,j,ncell)
     & - cellPhi(i+ii,j+jj,ncell)
          c3 = cellPhi(i+ii,j+jj,ncell)
     & - cellPhi(i+2*ii,j+2*jj,ncell)
        end if
        b0 = (13.d0 / 12.d0)*(c1-c0)**2+(0.25d0)*((3.0d0)*c1-c0)**2
        b1 = (13.d0 / 12.d0)*(c2-c1)**2+(0.25d0)*(c2+c1)**2
        b2 = (13.d0 / 12.d0)*(c3-c2)**2+(0.25d0)*(c3-(3.0d0)*c2)**2
        a0 = (0.3d0)/((eps*smoothFn(i,j,idir) + b0)**2)
        a1 = (0.6d0)/((eps*smoothFn(i,j,idir) + b1)**2)
        a2 = (0.1d0)/((eps*smoothFn(i,j,idir) + b2)**2)
        asuminv = (1.0d0)/(a0+a1+a2)
        w0 = a0*asuminv
        w1 = a1*asuminv
        w2 = a2*asuminv
        facePhi(i,j,n) = ( w0*v0 + w1*v1 + w2*v2 )
      enddo
      enddo
      enddo
      return
      end
      subroutine TVDFACE(
     & facePhi
     & ,ifacePhilo0,ifacePhilo1
     & ,ifacePhihi0,ifacePhihi1
     & ,nfacePhicomp
     & ,cellPhi
     & ,icellPhilo0,icellPhilo1
     & ,icellPhihi0,icellPhihi1
     & ,ncellPhicomp
     & ,faceVel
     & ,ifaceVello0,ifaceVello1
     & ,ifaceVelhi0,ifaceVelhi1
     & ,ifaceBoxlo0,ifaceBoxlo1
     & ,ifaceBoxhi0,ifaceBoxhi1
     & ,limiter
     & ,idir
     & )
      implicit none
      integer*8 ch_flops
      COMMON/ch_timer/ ch_flops
      integer CHF_ID(0:5,0:5)
      data CHF_ID/ 1,0,0,0,0,0 ,0,1,0,0,0,0 ,0,0,1,0,0,0 ,0,0,0,1,0,0 ,0
     &,0,0,0,1,0 ,0,0,0,0,0,1 /
      integer nfacePhicomp
      integer ifacePhilo0,ifacePhilo1
      integer ifacePhihi0,ifacePhihi1
      REAL*8 facePhi(
     & ifacePhilo0:ifacePhihi0,
     & ifacePhilo1:ifacePhihi1,
     & 0:nfacePhicomp-1)
      integer ncellPhicomp
      integer icellPhilo0,icellPhilo1
      integer icellPhihi0,icellPhihi1
      REAL*8 cellPhi(
     & icellPhilo0:icellPhihi0,
     & icellPhilo1:icellPhihi1,
     & 0:ncellPhicomp-1)
      integer ifaceVello0,ifaceVello1
      integer ifaceVelhi0,ifaceVelhi1
      REAL*8 faceVel(
     & ifaceVello0:ifaceVelhi0,
     & ifaceVello1:ifaceVelhi1)
      integer ifaceBoxlo0,ifaceBoxlo1
      integer ifaceBoxhi0,ifaceBoxhi1
      integer limiter
      integer idir
      integer n, ncell, i,j
      integer ii,jj
      REAL*8 val1st, val2nd, val, DeltaFluxL, DeltaFluxR
      ii = CHF_ID(idir, 0)
      jj = CHF_ID(idir, 1)
      do n=0, (nfacePhicomp-1)
        if(nfacePhicomp.lt.ncellPhicomp) then
           ncell = idir
        else
           ncell = n
        endif
      do j = ifaceBoxlo1,ifaceBoxhi1
      do i = ifaceBoxlo0,ifaceBoxhi0
        if( faceVel(i,j).gt.(0.0d0) ) then
          val1st = cellPhi(i-ii,j-jj,ncell)
          DeltaFluxL = (0.500d0)*cellPhi(i-ii,j-jj,ncell)
     & - (0.500d0)*cellPhi(i-2*ii,j-2*jj,ncell)
          DeltaFluxR = (0.500d0)*cellPhi(i,j,ncell)
     & - (0.500d0)*cellPhi(i-ii,j-jj,ncell)
        else
          val1st = cellPhi(i,j,ncell)
          DeltaFluxL = -(0.500d0)*cellPhi(i,j,ncell)
     & + (0.500d0)*cellPhi(i-ii,j-jj,ncell)
          DeltaFluxR = -(0.500d0)*cellPhi(i+ii,j+jj,ncell)
     & + (0.500d0)*cellPhi(i,j,ncell)
        end if
        if(limiter==1) then
          if ( (DeltaFluxL.gt.(0.0d0) .and. DeltaFluxR.gt.(0.0d0)) .or.
     & (DeltaFluxL.lt.(0.0d0) .and. DeltaFluxR.lt.(0.0d0)) ) then
            val2nd = (2.0d0)*DeltaFluxL*DeltaFluxR/(DeltaFluxL+DeltaFlux
     &R)
          else
            val2nd = (0.0d0)
          end if
        else if(limiter==2) then
          if (DeltaFluxL.gt.(0.0d0) .and. DeltaFluxR.gt.(0.0d0)) then
            val2nd = min(DeltaFluxL,DeltaFluxR)
          else if (DeltaFluxL.lt.(0.0d0) .and. DeltaFluxR.lt.(0.0d0)) th
     &en
            val2nd = max(DeltaFluxL,DeltaFluxR)
          else
            val2nd = (0.0d0)
          end if
        else if(limiter==3) then
          if (abs(DeltaFluxL).ge.abs(DeltaFluxR)) then
            DeltaFluxR = (2.0d0)*DeltaFluxR
          else
            DeltaFluxL = (2.0d0)*DeltaFluxL
          end if
          if (DeltaFluxL.gt.(0.0d0) .and. DeltaFluxR.gt.(0.0d0)) then
            val2nd = min(DeltaFluxL,DeltaFluxR)
          else if (DeltaFluxL.lt.(0.0d0) .and. DeltaFluxR.lt.(0.0d0)) th
     &en
            val2nd = max(DeltaFluxL,DeltaFluxR)
          else
            val2nd = (0.0d0)
          end if
        else
          val2nd = (0.0d0)
        end if
        facePhi(i,j,n) = val1st + val2nd
      enddo
      enddo
      enddo
      return
      end
      subroutine UW1FACE(
     & facePhi
     & ,ifacePhilo0,ifacePhilo1
     & ,ifacePhihi0,ifacePhihi1
     & ,nfacePhicomp
     & ,cellPhi
     & ,icellPhilo0,icellPhilo1
     & ,icellPhihi0,icellPhihi1
     & ,ncellPhicomp
     & ,faceVel
     & ,ifaceVello0,ifaceVello1
     & ,ifaceVelhi0,ifaceVelhi1
     & ,ifaceBoxlo0,ifaceBoxlo1
     & ,ifaceBoxhi0,ifaceBoxhi1
     & ,idir
     & )
      implicit none
      integer*8 ch_flops
      COMMON/ch_timer/ ch_flops
      integer CHF_ID(0:5,0:5)
      data CHF_ID/ 1,0,0,0,0,0 ,0,1,0,0,0,0 ,0,0,1,0,0,0 ,0,0,0,1,0,0 ,0
     &,0,0,0,1,0 ,0,0,0,0,0,1 /
      integer nfacePhicomp
      integer ifacePhilo0,ifacePhilo1
      integer ifacePhihi0,ifacePhihi1
      REAL*8 facePhi(
     & ifacePhilo0:ifacePhihi0,
     & ifacePhilo1:ifacePhihi1,
     & 0:nfacePhicomp-1)
      integer ncellPhicomp
      integer icellPhilo0,icellPhilo1
      integer icellPhihi0,icellPhihi1
      REAL*8 cellPhi(
     & icellPhilo0:icellPhihi0,
     & icellPhilo1:icellPhihi1,
     & 0:ncellPhicomp-1)
      integer ifaceVello0,ifaceVello1
      integer ifaceVelhi0,ifaceVelhi1
      REAL*8 faceVel(
     & ifaceVello0:ifaceVelhi0,
     & ifaceVello1:ifaceVelhi1)
      integer ifaceBoxlo0,ifaceBoxlo1
      integer ifaceBoxhi0,ifaceBoxhi1
      integer idir
      integer n, ncell, i,j
      integer ii,jj
      REAL*8 val
      ii = CHF_ID(idir, 0)
      jj = CHF_ID(idir, 1)
      do n=0, (nfacePhicomp-1)
        if(nfacePhicomp.lt.ncellPhicomp) then
           ncell = idir
        else
           ncell = n
        endif
      do j = ifaceBoxlo1,ifaceBoxhi1
      do i = ifaceBoxlo0,ifaceBoxhi0
        if( faceVel(i,j).gt.(0.0d0) ) then
          val = cellPhi(i-ii,j-jj,ncell)
        else
          val = cellPhi(i,j,ncell)
        end if
        facePhi(i,j,n) = val
      enddo
      enddo
      enddo
      return
      end
      subroutine UW1C2FACE(
     & facePhi
     & ,ifacePhilo0,ifacePhilo1
     & ,ifacePhihi0,ifacePhihi1
     & ,nfacePhicomp
     & ,cellPhi
     & ,icellPhilo0,icellPhilo1
     & ,icellPhihi0,icellPhihi1
     & ,ncellPhicomp
     & ,cellFun
     & ,icellFunlo0,icellFunlo1
     & ,icellFunhi0,icellFunhi1
     & ,ncellFuncomp
     & ,faceVel
     & ,ifaceVello0,ifaceVello1
     & ,ifaceVelhi0,ifaceVelhi1
     & ,ifaceBoxlo0,ifaceBoxlo1
     & ,ifaceBoxhi0,ifaceBoxhi1
     & ,limType
     & ,idir
     & )
      implicit none
      integer*8 ch_flops
      COMMON/ch_timer/ ch_flops
      integer CHF_ID(0:5,0:5)
      data CHF_ID/ 1,0,0,0,0,0 ,0,1,0,0,0,0 ,0,0,1,0,0,0 ,0,0,0,1,0,0 ,0
     &,0,0,0,1,0 ,0,0,0,0,0,1 /
      integer nfacePhicomp
      integer ifacePhilo0,ifacePhilo1
      integer ifacePhihi0,ifacePhihi1
      REAL*8 facePhi(
     & ifacePhilo0:ifacePhihi0,
     & ifacePhilo1:ifacePhihi1,
     & 0:nfacePhicomp-1)
      integer ncellPhicomp
      integer icellPhilo0,icellPhilo1
      integer icellPhihi0,icellPhihi1
      REAL*8 cellPhi(
     & icellPhilo0:icellPhihi0,
     & icellPhilo1:icellPhihi1,
     & 0:ncellPhicomp-1)
      integer ncellFuncomp
      integer icellFunlo0,icellFunlo1
      integer icellFunhi0,icellFunhi1
      REAL*8 cellFun(
     & icellFunlo0:icellFunhi0,
     & icellFunlo1:icellFunhi1,
     & 0:ncellFuncomp-1)
      integer ifaceVello0,ifaceVello1
      integer ifaceVelhi0,ifaceVelhi1
      REAL*8 faceVel(
     & ifaceVello0:ifaceVelhi0,
     & ifaceVello1:ifaceVelhi1)
      integer ifaceBoxlo0,ifaceBoxlo1
      integer ifaceBoxhi0,ifaceBoxhi1
      integer limType
      integer idir
      integer n, ncell, i,j
      integer ii,jj
      REAL*8 val, valUW1, valC2, numer, denom, rlim, lim0, lim1, limiter
      limiter = 1.0
      ii = CHF_ID(idir, 0)
      jj = CHF_ID(idir, 1)
      do n=0, (nfacePhicomp-1)
        if(nfacePhicomp.lt.ncellPhicomp) then
           ncell = idir
        else
           ncell = n
        endif
      do j = ifaceBoxlo1,ifaceBoxhi1
      do i = ifaceBoxlo0,ifaceBoxhi0
           numer = cellFun(i-ii,j-jj,ncell)
     & - cellFun(i-2*ii,j-2*jj,ncell)
           denom = cellFun(i,j,ncell)
     & - cellFun(i-ii,j-jj,ncell)
           if( denom.eq.(0.0d0) ) then
              rlim = 1000.0
           else
              rlim = numer/denom
           end if
           if( limType.eq.0 ) then
              limiter = 0.0
           else if( limType.eq.1) then
              limiter = (rlim + abs(rlim))/(1.0 + abs(rlim))
           else if( limType.eq.2) then
              limiter = min(1.0,rlim)
           else if( limType.eq.3) then
              lim0 = min(rlim,2.0)
              lim1 = min(2.0*rlim,1.0)
              limiter = max(lim0,lim1)
           else if( limType.eq.4) then
              limiter = (rlim*rlim + rlim)/(rlim*rlim + 1.0)
           else if( limType.eq.5) then
              limiter = (2.0*rlim)/(rlim*rlim + 1.0)
           else
              limiter = 1.0
           end if
           limiter = max(0.0,limiter)
           valC2 = (0.500d0)*( cellPhi(i-ii,j-jj,ncell)
     & + cellPhi(i,j,ncell) )
           if( faceVel(i,j).gt.(0.0d0) ) then
              valUW1 = cellPhi(i-ii,j-jj,ncell)
           else
              valUW1 = cellPhi(i,j,ncell)
           end if
           facePhi(i,j,n) = valUW1 + limiter*(valC2 - valUW1);
      enddo
      enddo
      enddo
      return
      end
      subroutine UW3FACE(
     & facePhi
     & ,ifacePhilo0,ifacePhilo1
     & ,ifacePhihi0,ifacePhihi1
     & ,nfacePhicomp
     & ,cellPhi
     & ,icellPhilo0,icellPhilo1
     & ,icellPhihi0,icellPhihi1
     & ,ncellPhicomp
     & ,faceVel
     & ,ifaceVello0,ifaceVello1
     & ,ifaceVelhi0,ifaceVelhi1
     & ,ifaceBoxlo0,ifaceBoxlo1
     & ,ifaceBoxhi0,ifaceBoxhi1
     & ,idir
     & )
      implicit none
      integer*8 ch_flops
      COMMON/ch_timer/ ch_flops
      integer CHF_ID(0:5,0:5)
      data CHF_ID/ 1,0,0,0,0,0 ,0,1,0,0,0,0 ,0,0,1,0,0,0 ,0,0,0,1,0,0 ,0
     &,0,0,0,1,0 ,0,0,0,0,0,1 /
      integer nfacePhicomp
      integer ifacePhilo0,ifacePhilo1
      integer ifacePhihi0,ifacePhihi1
      REAL*8 facePhi(
     & ifacePhilo0:ifacePhihi0,
     & ifacePhilo1:ifacePhihi1,
     & 0:nfacePhicomp-1)
      integer ncellPhicomp
      integer icellPhilo0,icellPhilo1
      integer icellPhihi0,icellPhihi1
      REAL*8 cellPhi(
     & icellPhilo0:icellPhihi0,
     & icellPhilo1:icellPhihi1,
     & 0:ncellPhicomp-1)
      integer ifaceVello0,ifaceVello1
      integer ifaceVelhi0,ifaceVelhi1
      REAL*8 faceVel(
     & ifaceVello0:ifaceVelhi0,
     & ifaceVello1:ifaceVelhi1)
      integer ifaceBoxlo0,ifaceBoxlo1
      integer ifaceBoxhi0,ifaceBoxhi1
      integer idir
      integer n, ncell, i,j
      integer ii,jj
      REAL*8 val
      ii = CHF_ID(idir, 0)
      jj = CHF_ID(idir, 1)
      do n=0, (nfacePhicomp-1)
        if(nfacePhicomp.lt.ncellPhicomp) then
           ncell = idir
        else
           ncell = n
        endif
      do j = ifaceBoxlo1,ifaceBoxhi1
      do i = ifaceBoxlo0,ifaceBoxhi0
        if( faceVel(i,j).gt.(0.0d0) ) then
          val = - (1.0d0)
     & * cellPhi(i-2*ii,j-2*jj,ncell)
     & + (5.0d0)
     & * cellPhi(i-ii,j-jj,ncell)
     & + (2.0d0)
     & * cellPhi(i,j,ncell)
        else
          val = - (1.0d0)
     & * cellPhi(i+ii,j+jj,ncell)
     & + (5.0d0)
     & * cellPhi(i,j,ncell)
     & + (2.0d0)
     & * cellPhi(i-ii,j-jj,ncell)
        end if
        facePhi(i,j,n) = val * (1.000d0 / 6.000d0)
      enddo
      enddo
      enddo
      return
      end
      subroutine UW5FACE(
     & facePhi
     & ,ifacePhilo0,ifacePhilo1
     & ,ifacePhihi0,ifacePhihi1
     & ,nfacePhicomp
     & ,cellPhi
     & ,icellPhilo0,icellPhilo1
     & ,icellPhihi0,icellPhihi1
     & ,ncellPhicomp
     & ,faceVel
     & ,ifaceVello0,ifaceVello1
     & ,ifaceVelhi0,ifaceVelhi1
     & ,ifaceBoxlo0,ifaceBoxlo1
     & ,ifaceBoxhi0,ifaceBoxhi1
     & ,idir
     & )
      implicit none
      integer*8 ch_flops
      COMMON/ch_timer/ ch_flops
      integer CHF_ID(0:5,0:5)
      data CHF_ID/ 1,0,0,0,0,0 ,0,1,0,0,0,0 ,0,0,1,0,0,0 ,0,0,0,1,0,0 ,0
     &,0,0,0,1,0 ,0,0,0,0,0,1 /
      integer nfacePhicomp
      integer ifacePhilo0,ifacePhilo1
      integer ifacePhihi0,ifacePhihi1
      REAL*8 facePhi(
     & ifacePhilo0:ifacePhihi0,
     & ifacePhilo1:ifacePhihi1,
     & 0:nfacePhicomp-1)
      integer ncellPhicomp
      integer icellPhilo0,icellPhilo1
      integer icellPhihi0,icellPhihi1
      REAL*8 cellPhi(
     & icellPhilo0:icellPhihi0,
     & icellPhilo1:icellPhihi1,
     & 0:ncellPhicomp-1)
      integer ifaceVello0,ifaceVello1
      integer ifaceVelhi0,ifaceVelhi1
      REAL*8 faceVel(
     & ifaceVello0:ifaceVelhi0,
     & ifaceVello1:ifaceVelhi1)
      integer ifaceBoxlo0,ifaceBoxlo1
      integer ifaceBoxhi0,ifaceBoxhi1
      integer idir
      integer n, ncell, i,j
      integer ii,jj
      REAL*8 val
      ii = CHF_ID(idir, 0)
      jj = CHF_ID(idir, 1)
      do n=0, (nfacePhicomp-1)
         if(nfacePhicomp.lt.ncellPhicomp) then
            ncell = idir
         else
            ncell = n
         endif
      do j = ifaceBoxlo1,ifaceBoxhi1
      do i = ifaceBoxlo0,ifaceBoxhi0
        if( faceVel(i,j).gt.(0.0d0) ) then
          val = (2.0d0)
     & * cellPhi(i-3*ii,j-3*jj,ncell)
     & - (13.0d0)
     & * cellPhi(i-2*ii,j-2*jj,ncell)
     & + (47.0d0)
     & * cellPhi(i-ii,j-jj,ncell)
     & + (27.0d0)
     & * cellPhi(i,j,ncell)
     & - (3.0d0)
     & * cellPhi(i+ii,j+jj,ncell)
        else
          val = (2.0d0)
     & * cellPhi(i+2*ii,j+2*jj,ncell)
     & - (13.0d0)
     & * cellPhi(i+ii,j+jj,ncell)
     & + (47.0d0)
     & * cellPhi(i,j,ncell)
     & + (27.0d0)
     & * cellPhi(i-ii,j-jj,ncell)
     & - (3.0d0)
     & * cellPhi(i-2*ii,j-2*jj,ncell)
         end if
        facePhi(i,j,n) = val * (1.0d0 / 60.0d0)
      enddo
      enddo
      enddo
      return
      end
      subroutine QUICKFACE(
     & facePhi
     & ,ifacePhilo0,ifacePhilo1
     & ,ifacePhihi0,ifacePhihi1
     & ,nfacePhicomp
     & ,cellPhi
     & ,icellPhilo0,icellPhilo1
     & ,icellPhihi0,icellPhihi1
     & ,ncellPhicomp
     & ,faceVel
     & ,ifaceVello0,ifaceVello1
     & ,ifaceVelhi0,ifaceVelhi1
     & ,ifaceBoxlo0,ifaceBoxlo1
     & ,ifaceBoxhi0,ifaceBoxhi1
     & ,idir
     & )
      implicit none
      integer*8 ch_flops
      COMMON/ch_timer/ ch_flops
      integer CHF_ID(0:5,0:5)
      data CHF_ID/ 1,0,0,0,0,0 ,0,1,0,0,0,0 ,0,0,1,0,0,0 ,0,0,0,1,0,0 ,0
     &,0,0,0,1,0 ,0,0,0,0,0,1 /
      integer nfacePhicomp
      integer ifacePhilo0,ifacePhilo1
      integer ifacePhihi0,ifacePhihi1
      REAL*8 facePhi(
     & ifacePhilo0:ifacePhihi0,
     & ifacePhilo1:ifacePhihi1,
     & 0:nfacePhicomp-1)
      integer ncellPhicomp
      integer icellPhilo0,icellPhilo1
      integer icellPhihi0,icellPhihi1
      REAL*8 cellPhi(
     & icellPhilo0:icellPhihi0,
     & icellPhilo1:icellPhihi1,
     & 0:ncellPhicomp-1)
      integer ifaceVello0,ifaceVello1
      integer ifaceVelhi0,ifaceVelhi1
      REAL*8 faceVel(
     & ifaceVello0:ifaceVelhi0,
     & ifaceVello1:ifaceVelhi1)
      integer ifaceBoxlo0,ifaceBoxlo1
      integer ifaceBoxhi0,ifaceBoxhi1
      integer idir
      integer n, ncell, i,j
      integer ii,jj
      REAL*8 val
      ii = CHF_ID(idir, 0)
      jj = CHF_ID(idir, 1)
      do n=0, (nfacePhicomp-1)
        if(nfacePhicomp.lt.ncellPhicomp) then
           ncell = idir
        else
           ncell = n
        endif
      do j = ifaceBoxlo1,ifaceBoxhi1
      do i = ifaceBoxlo0,ifaceBoxhi0
        if( faceVel(i,j).gt.(0.0d0) ) then
          val = - (1.0d0)
     & * cellPhi(i-2*ii,j-2*jj,ncell)
     & + (6.0d0)
     & * cellPhi(i-ii,j-jj,ncell)
     & + (3.0d0)
     & * cellPhi(i,j,ncell)
        else
          val = - (1.0d0)
     & * cellPhi(i+ii,j+jj,ncell)
     & + (6.0d0)
     & * cellPhi(i,j,ncell)
     & + (3.0d0)
     & * cellPhi(i-ii,j-jj,ncell)
        end if
        facePhi(i,j,n) = val * (0.125d0)
      enddo
      enddo
      enddo
      return
      end
      subroutine C2FACETOCELL(
     & icellBoxlo0,icellBoxlo1
     & ,icellBoxhi0,icellBoxhi1
     & ,dir
     & ,facePhi
     & ,ifacePhilo0,ifacePhilo1
     & ,ifacePhihi0,ifacePhihi1
     & ,cellPhi
     & ,icellPhilo0,icellPhilo1
     & ,icellPhihi0,icellPhihi1
     & )
      implicit none
      integer*8 ch_flops
      COMMON/ch_timer/ ch_flops
      integer CHF_ID(0:5,0:5)
      data CHF_ID/ 1,0,0,0,0,0 ,0,1,0,0,0,0 ,0,0,1,0,0,0 ,0,0,0,1,0,0 ,0
     &,0,0,0,1,0 ,0,0,0,0,0,1 /
      integer icellBoxlo0,icellBoxlo1
      integer icellBoxhi0,icellBoxhi1
      integer dir
      integer ifacePhilo0,ifacePhilo1
      integer ifacePhihi0,ifacePhihi1
      REAL*8 facePhi(
     & ifacePhilo0:ifacePhihi0,
     & ifacePhilo1:ifacePhihi1)
      integer icellPhilo0,icellPhilo1
      integer icellPhihi0,icellPhihi1
      REAL*8 cellPhi(
     & icellPhilo0:icellPhihi0,
     & icellPhilo1:icellPhihi1)
      integer i,j
      integer ii,jj
      REAL*8 val
      ii = CHF_ID(dir, 0)
      jj = CHF_ID(dir, 1)
      do j = icellBoxlo1,icellBoxhi1
      do i = icellBoxlo0,icellBoxhi0
         val = facePhi(i+ii,j+jj)
     & + facePhi(i,j)
         cellPhi(i,j) = val * (0.500d0)
      enddo
      enddo
      return
      end
      subroutine FACE_SCALAR_TO_CELL(
     & icellBoxlo0,icellBoxlo1
     & ,icellBoxhi0,icellBoxhi1
     & ,dir
     & ,facePhi
     & ,ifacePhilo0,ifacePhilo1
     & ,ifacePhihi0,ifacePhihi1
     & ,nfacePhicomp
     & ,cellPhi
     & ,icellPhilo0,icellPhilo1
     & ,icellPhihi0,icellPhihi1
     & ,ncellPhicomp
     & )
      implicit none
      integer*8 ch_flops
      COMMON/ch_timer/ ch_flops
      integer CHF_ID(0:5,0:5)
      data CHF_ID/ 1,0,0,0,0,0 ,0,1,0,0,0,0 ,0,0,1,0,0,0 ,0,0,0,1,0,0 ,0
     &,0,0,0,1,0 ,0,0,0,0,0,1 /
      integer icellBoxlo0,icellBoxlo1
      integer icellBoxhi0,icellBoxhi1
      integer dir
      integer nfacePhicomp
      integer ifacePhilo0,ifacePhilo1
      integer ifacePhihi0,ifacePhihi1
      REAL*8 facePhi(
     & ifacePhilo0:ifacePhihi0,
     & ifacePhilo1:ifacePhihi1,
     & 0:nfacePhicomp-1)
      integer ncellPhicomp
      integer icellPhilo0,icellPhilo1
      integer icellPhihi0,icellPhihi1
      REAL*8 cellPhi(
     & icellPhilo0:icellPhihi0,
     & icellPhilo1:icellPhihi1,
     & 0:ncellPhicomp-1)
      integer comp, i,j
      integer ii,jj
      REAL*8 val
      ii = CHF_ID(dir, 0)
      jj = CHF_ID(dir, 1)
      do j = icellBoxlo1,icellBoxhi1
      do i = icellBoxlo0,icellBoxhi0
         do comp = 0, (ncellPhicomp-1)
            val = (0.500d0)*( facePhi(i+ii,j+jj,comp)
     & + facePhi(i,j,comp) )
            cellPhi(i,j,comp) = (cellPhi(i,j,comp) + val)/(dir+1.0)
         enddo
      enddo
      enddo
      return
      end
      subroutine C2_EDGES_TO_EDGES(
     & iedgeBoxlo0,iedgeBoxlo1
     & ,iedgeBoxhi0,iedgeBoxhi1
     & ,Ein
     & ,iEinlo0,iEinlo1
     & ,iEinhi0,iEinhi1
     & ,Eout
     & ,iEoutlo0,iEoutlo1
     & ,iEouthi0,iEouthi1
     & ,idir
     & )
      implicit none
      integer*8 ch_flops
      COMMON/ch_timer/ ch_flops
      integer CHF_ID(0:5,0:5)
      data CHF_ID/ 1,0,0,0,0,0 ,0,1,0,0,0,0 ,0,0,1,0,0,0 ,0,0,0,1,0,0 ,0
     &,0,0,0,1,0 ,0,0,0,0,0,1 /
      integer iedgeBoxlo0,iedgeBoxlo1
      integer iedgeBoxhi0,iedgeBoxhi1
      integer iEinlo0,iEinlo1
      integer iEinhi0,iEinhi1
      REAL*8 Ein(
     & iEinlo0:iEinhi0,
     & iEinlo1:iEinhi1)
      integer iEoutlo0,iEoutlo1
      integer iEouthi0,iEouthi1
      REAL*8 Eout(
     & iEoutlo0:iEouthi0,
     & iEoutlo1:iEouthi1)
      integer idir
      integer n, ncell, i,j
      integer ii,jj
      integer ii0,jj0
      REAL*8 val_up, val_down
      ii = 1 - CHF_ID(idir, 0)
      jj = 1 - CHF_ID(idir, 1)
      ii0 = CHF_ID(idir, 0)
      jj0 = CHF_ID(idir, 1)
      do j = iedgeBoxlo1,iedgeBoxhi1
      do i = iedgeBoxlo0,iedgeBoxhi0
         val_up = Ein(i-ii,j-jj)
     & + Ein(i,j)
         val_up = val_up * (0.500d0)
         val_down = Ein(i-ii+ii0,j-jj+jj0)
     & + Ein(i+ii0,j+jj0)
         val_down = val_down * (0.500d0)
         Eout(i,j) = ( val_down + val_up ) * (0.500d0)
      enddo
      enddo
      return
      end
      subroutine C2_NODES_TO_EDGES(
     & iedgeBoxlo0,iedgeBoxlo1
     & ,iedgeBoxhi0,iedgeBoxhi1
     & ,dir
     & ,ncFin
     & ,incFinlo0,incFinlo1
     & ,incFinhi0,incFinhi1
     & ,nncFincomp
     & ,ecFout
     & ,iecFoutlo0,iecFoutlo1
     & ,iecFouthi0,iecFouthi1
     & ,necFoutcomp
     & )
      implicit none
      integer*8 ch_flops
      COMMON/ch_timer/ ch_flops
      integer CHF_ID(0:5,0:5)
      data CHF_ID/ 1,0,0,0,0,0 ,0,1,0,0,0,0 ,0,0,1,0,0,0 ,0,0,0,1,0,0 ,0
     &,0,0,0,1,0 ,0,0,0,0,0,1 /
      integer iedgeBoxlo0,iedgeBoxlo1
      integer iedgeBoxhi0,iedgeBoxhi1
      integer dir
      integer nncFincomp
      integer incFinlo0,incFinlo1
      integer incFinhi0,incFinhi1
      REAL*8 ncFin(
     & incFinlo0:incFinhi0,
     & incFinlo1:incFinhi1,
     & 0:nncFincomp-1)
      integer necFoutcomp
      integer iecFoutlo0,iecFoutlo1
      integer iecFouthi0,iecFouthi1
      REAL*8 ecFout(
     & iecFoutlo0:iecFouthi0,
     & iecFoutlo1:iecFouthi1,
     & 0:necFoutcomp-1)
      integer comp, n, ncell, i,j
      integer ii,jj
      REAL*8 valUp, valDown
      ii = CHF_ID(dir, 0)
      jj = CHF_ID(dir, 1)
      do j = iedgeBoxlo1,iedgeBoxhi1
      do i = iedgeBoxlo0,iedgeBoxhi0
         do comp = 0, (necFoutcomp-1)
            valUp = ncFin(i+ii,j+jj,comp)
            valDown = ncFin(i,j,comp)
            ecFout(i,j,comp) = ( valUp + valDown ) * (0.500d0)
         enddo
      enddo
      enddo
      return
      end
      subroutine C2_NODES_TO_CELLS(
     & icellBoxlo0,icellBoxlo1
     & ,icellBoxhi0,icellBoxhi1
     & ,ncFin
     & ,incFinlo0,incFinlo1
     & ,incFinhi0,incFinhi1
     & ,nncFincomp
     & ,ccFout
     & ,iccFoutlo0,iccFoutlo1
     & ,iccFouthi0,iccFouthi1
     & ,nccFoutcomp
     & )
      implicit none
      integer*8 ch_flops
      COMMON/ch_timer/ ch_flops
      integer icellBoxlo0,icellBoxlo1
      integer icellBoxhi0,icellBoxhi1
      integer nncFincomp
      integer incFinlo0,incFinlo1
      integer incFinhi0,incFinhi1
      REAL*8 ncFin(
     & incFinlo0:incFinhi0,
     & incFinlo1:incFinhi1,
     & 0:nncFincomp-1)
      integer nccFoutcomp
      integer iccFoutlo0,iccFoutlo1
      integer iccFouthi0,iccFouthi1
      REAL*8 ccFout(
     & iccFoutlo0:iccFouthi0,
     & iccFoutlo1:iccFouthi1,
     & 0:nccFoutcomp-1)
      integer comp, n, ncell, i,j
      integer ii,jj
      REAL*8 val00, val01, val10, val11
      do j = icellBoxlo1,icellBoxhi1
      do i = icellBoxlo0,icellBoxhi0
         do comp = 0, (nccFoutcomp-1)
            val11 = ncFin(i+1,j+1,comp)
            val10 = ncFin(i+1,j,comp)
            val01 = ncFin(i,j+1,comp)
            val00 = ncFin(i,j,comp)
            ccFout(i,j,comp) = ( val00 + val01 + val10 + val11 ) / (4.0d
     &0)
         enddo
      enddo
      enddo
      return
      end
      subroutine C2_CELLS_TO_NODES(
     & inodeBoxlo0,inodeBoxlo1
     & ,inodeBoxhi0,inodeBoxhi1
     & ,ccFin
     & ,iccFinlo0,iccFinlo1
     & ,iccFinhi0,iccFinhi1
     & ,nccFincomp
     & ,ncFout
     & ,incFoutlo0,incFoutlo1
     & ,incFouthi0,incFouthi1
     & ,nncFoutcomp
     & )
      implicit none
      integer*8 ch_flops
      COMMON/ch_timer/ ch_flops
      integer inodeBoxlo0,inodeBoxlo1
      integer inodeBoxhi0,inodeBoxhi1
      integer nccFincomp
      integer iccFinlo0,iccFinlo1
      integer iccFinhi0,iccFinhi1
      REAL*8 ccFin(
     & iccFinlo0:iccFinhi0,
     & iccFinlo1:iccFinhi1,
     & 0:nccFincomp-1)
      integer nncFoutcomp
      integer incFoutlo0,incFoutlo1
      integer incFouthi0,incFouthi1
      REAL*8 ncFout(
     & incFoutlo0:incFouthi0,
     & incFoutlo1:incFouthi1,
     & 0:nncFoutcomp-1)
      integer comp, n, ncell, i,j
      integer ii,jj
      REAL*8 val00, val01, val10, val11
      do j = inodeBoxlo1,inodeBoxhi1
      do i = inodeBoxlo0,inodeBoxhi0
         do comp = 0, (nncFoutcomp-1)
            val11 = ccFin(i,j,comp)
            val10 = ccFin(i-1,j,comp)
            val01 = ccFin(i,j-1,comp)
            val00 = ccFin(i-1,j-1,comp)
            ncFout(i,j,comp) = ( val00 + val01 + val10 + val11 ) / (4.0d
     &0)
         enddo
      enddo
      enddo
      return
      end
      subroutine EDGE_GRAD_AT_CELLS(
     & iboxlo0,iboxlo1
     & ,iboxhi0,iboxhi1
     & ,dir
     & ,dX
     & ,PhiOnEdges
     & ,iPhiOnEdgeslo0,iPhiOnEdgeslo1
     & ,iPhiOnEdgeshi0,iPhiOnEdgeshi1
     & ,gradPhi
     & ,igradPhilo0,igradPhilo1
     & ,igradPhihi0,igradPhihi1
     & )
      implicit none
      integer*8 ch_flops
      COMMON/ch_timer/ ch_flops
      integer CHF_ID(0:5,0:5)
      data CHF_ID/ 1,0,0,0,0,0 ,0,1,0,0,0,0 ,0,0,1,0,0,0 ,0,0,0,1,0,0 ,0
     &,0,0,0,1,0 ,0,0,0,0,0,1 /
      integer iboxlo0,iboxlo1
      integer iboxhi0,iboxhi1
      integer dir
      REAL*8 dX
      integer iPhiOnEdgeslo0,iPhiOnEdgeslo1
      integer iPhiOnEdgeshi0,iPhiOnEdgeshi1
      REAL*8 PhiOnEdges(
     & iPhiOnEdgeslo0:iPhiOnEdgeshi0,
     & iPhiOnEdgeslo1:iPhiOnEdgeshi1)
      integer igradPhilo0,igradPhilo1
      integer igradPhihi0,igradPhihi1
      REAL*8 gradPhi(
     & igradPhilo0:igradPhihi0,
     & igradPhilo1:igradPhihi1)
      integer i,j
      integer ii,jj
      double precision edgePhi_up, edgePhi_down
      ii = CHF_ID(dir, 0)
      jj = CHF_ID(dir, 1)
      do j = iboxlo1,iboxhi1
      do i = iboxlo0,iboxhi0
         edgePhi_down = PhiOnEdges(i,j)
         edgePhi_up = PhiOnEdges(i+ii,j+jj)
         gradPhi(i,j) = (edgePhi_up - edgePhi_down)/dX
      enddo
      enddo
      return
      end
      subroutine EDGE_GRAD_AT_NODES(
     & iboxlo0,iboxlo1
     & ,iboxhi0,iboxhi1
     & ,dir
     & ,dX
     & ,PhiOnEdges
     & ,iPhiOnEdgeslo0,iPhiOnEdgeslo1
     & ,iPhiOnEdgeshi0,iPhiOnEdgeshi1
     & ,gradPhi
     & ,igradPhilo0,igradPhilo1
     & ,igradPhihi0,igradPhihi1
     & )
      implicit none
      integer*8 ch_flops
      COMMON/ch_timer/ ch_flops
      integer CHF_ID(0:5,0:5)
      data CHF_ID/ 1,0,0,0,0,0 ,0,1,0,0,0,0 ,0,0,1,0,0,0 ,0,0,0,1,0,0 ,0
     &,0,0,0,1,0 ,0,0,0,0,0,1 /
      integer iboxlo0,iboxlo1
      integer iboxhi0,iboxhi1
      integer dir
      REAL*8 dX
      integer iPhiOnEdgeslo0,iPhiOnEdgeslo1
      integer iPhiOnEdgeshi0,iPhiOnEdgeshi1
      REAL*8 PhiOnEdges(
     & iPhiOnEdgeslo0:iPhiOnEdgeshi0,
     & iPhiOnEdgeslo1:iPhiOnEdgeshi1)
      integer igradPhilo0,igradPhilo1
      integer igradPhihi0,igradPhihi1
      REAL*8 gradPhi(
     & igradPhilo0:igradPhihi0,
     & igradPhilo1:igradPhihi1)
      integer i,j
      integer ii,jj
      double precision edgePhi_up, edgePhi_down
      ii = CHF_ID(dir, 0)
      jj = CHF_ID(dir, 1)
      do j = iboxlo1,iboxhi1
      do i = iboxlo0,iboxhi0
         edgePhi_down = PhiOnEdges(i-ii,j-jj)
         edgePhi_up = PhiOnEdges(i,j)
         gradPhi(i,j) = (edgePhi_up - edgePhi_down)/dX
      enddo
      enddo
      return
      end
      subroutine C2CELL(
     & icellBoxlo0,icellBoxlo1
     & ,icellBoxhi0,icellBoxhi1
     & ,dir
     & ,edgePhi
     & ,iedgePhilo0,iedgePhilo1
     & ,iedgePhihi0,iedgePhihi1
     & ,cellPhi
     & ,icellPhilo0,icellPhilo1
     & ,icellPhihi0,icellPhihi1
     & )
      implicit none
      integer*8 ch_flops
      COMMON/ch_timer/ ch_flops
      integer CHF_ID(0:5,0:5)
      data CHF_ID/ 1,0,0,0,0,0 ,0,1,0,0,0,0 ,0,0,1,0,0,0 ,0,0,0,1,0,0 ,0
     &,0,0,0,1,0 ,0,0,0,0,0,1 /
      integer icellBoxlo0,icellBoxlo1
      integer icellBoxhi0,icellBoxhi1
      integer dir
      integer iedgePhilo0,iedgePhilo1
      integer iedgePhihi0,iedgePhihi1
      REAL*8 edgePhi(
     & iedgePhilo0:iedgePhihi0,
     & iedgePhilo1:iedgePhihi1)
      integer icellPhilo0,icellPhilo1
      integer icellPhihi0,icellPhihi1
      REAL*8 cellPhi(
     & icellPhilo0:icellPhihi0,
     & icellPhilo1:icellPhihi1)
      integer i,j
      integer ii,jj
      REAL*8 val
      ii = 1 - CHF_ID(dir, 0)
      jj = 1 - CHF_ID(dir, 1)
      do j = icellBoxlo1,icellBoxhi1
      do i = icellBoxlo0,icellBoxhi0
         val = edgePhi(i+ii,j+jj)
     & + edgePhi(i,j)
         cellPhi(i,j) = val * (0.500d0)
      enddo
      enddo
      return
      end
      subroutine EDGE_SCALAR_TO_CELL(
     & icellBoxlo0,icellBoxlo1
     & ,icellBoxhi0,icellBoxhi1
     & ,dir
     & ,edgePhi
     & ,iedgePhilo0,iedgePhilo1
     & ,iedgePhihi0,iedgePhihi1
     & ,nedgePhicomp
     & ,cellPhi
     & ,icellPhilo0,icellPhilo1
     & ,icellPhihi0,icellPhihi1
     & ,ncellPhicomp
     & )
      implicit none
      integer*8 ch_flops
      COMMON/ch_timer/ ch_flops
      integer CHF_ID(0:5,0:5)
      data CHF_ID/ 1,0,0,0,0,0 ,0,1,0,0,0,0 ,0,0,1,0,0,0 ,0,0,0,1,0,0 ,0
     &,0,0,0,1,0 ,0,0,0,0,0,1 /
      integer icellBoxlo0,icellBoxlo1
      integer icellBoxhi0,icellBoxhi1
      integer dir
      integer nedgePhicomp
      integer iedgePhilo0,iedgePhilo1
      integer iedgePhihi0,iedgePhihi1
      REAL*8 edgePhi(
     & iedgePhilo0:iedgePhihi0,
     & iedgePhilo1:iedgePhihi1,
     & 0:nedgePhicomp-1)
      integer ncellPhicomp
      integer icellPhilo0,icellPhilo1
      integer icellPhihi0,icellPhihi1
      REAL*8 cellPhi(
     & icellPhilo0:icellPhihi0,
     & icellPhilo1:icellPhihi1,
     & 0:ncellPhicomp-1)
      integer comp, i,j
      integer ii,jj
      REAL*8 val
      ii = 1-CHF_ID(dir, 0)
      jj = 1-CHF_ID(dir, 1)
      do j = icellBoxlo1,icellBoxhi1
      do i = icellBoxlo0,icellBoxhi0
         do comp = 0, (ncellPhicomp-1)
            val = (0.500d0)*( edgePhi(i+ii,j+jj,comp)
     & + edgePhi(i,j,comp) )
            cellPhi(i,j,comp) = (cellPhi(i,j,comp) + val)/(dir+1.0)
         enddo
      enddo
      enddo
      return
      end
      subroutine C2EDGE(
     & edgePhi
     & ,iedgePhilo0,iedgePhilo1
     & ,iedgePhihi0,iedgePhihi1
     & ,nedgePhicomp
     & ,cellPhi
     & ,icellPhilo0,icellPhilo1
     & ,icellPhihi0,icellPhihi1
     & ,ncellPhicomp
     & ,iedgeBoxlo0,iedgeBoxlo1
     & ,iedgeBoxhi0,iedgeBoxhi1
     & ,idir
     & )
      implicit none
      integer*8 ch_flops
      COMMON/ch_timer/ ch_flops
      integer CHF_ID(0:5,0:5)
      data CHF_ID/ 1,0,0,0,0,0 ,0,1,0,0,0,0 ,0,0,1,0,0,0 ,0,0,0,1,0,0 ,0
     &,0,0,0,1,0 ,0,0,0,0,0,1 /
      integer nedgePhicomp
      integer iedgePhilo0,iedgePhilo1
      integer iedgePhihi0,iedgePhihi1
      REAL*8 edgePhi(
     & iedgePhilo0:iedgePhihi0,
     & iedgePhilo1:iedgePhihi1,
     & 0:nedgePhicomp-1)
      integer ncellPhicomp
      integer icellPhilo0,icellPhilo1
      integer icellPhihi0,icellPhihi1
      REAL*8 cellPhi(
     & icellPhilo0:icellPhihi0,
     & icellPhilo1:icellPhihi1,
     & 0:ncellPhicomp-1)
      integer iedgeBoxlo0,iedgeBoxlo1
      integer iedgeBoxhi0,iedgeBoxhi1
      integer idir
      integer n, ncell, i,j
      integer ii,jj
      REAL*8 val
      ii = 1 - CHF_ID(idir, 0)
      jj = 1 - CHF_ID(idir, 1)
      do n=0, (nedgePhicomp-1)
         if(nedgePhicomp.lt.ncellPhicomp) then
            ncell = idir
         else
            ncell = n
         endif
      do j = iedgeBoxlo1,iedgeBoxhi1
      do i = iedgeBoxlo0,iedgeBoxhi0
            val = cellPhi(i-ii,j-jj,ncell)
     & + cellPhi(i,j,ncell)
            edgePhi(i,j,n) = val * (0.500d0)
      enddo
      enddo
      enddo
      return
      end
      subroutine UW1EDGE(
     & edgePhi
     & ,iedgePhilo0,iedgePhilo1
     & ,iedgePhihi0,iedgePhihi1
     & ,nedgePhicomp
     & ,cellPhi
     & ,icellPhilo0,icellPhilo1
     & ,icellPhihi0,icellPhihi1
     & ,ncellPhicomp
     & ,faceVel
     & ,ifaceVello0,ifaceVello1
     & ,ifaceVelhi0,ifaceVelhi1
     & ,iedgeBoxlo0,iedgeBoxlo1
     & ,iedgeBoxhi0,iedgeBoxhi1
     & ,idir
     & )
      implicit none
      integer*8 ch_flops
      COMMON/ch_timer/ ch_flops
      integer CHF_ID(0:5,0:5)
      data CHF_ID/ 1,0,0,0,0,0 ,0,1,0,0,0,0 ,0,0,1,0,0,0 ,0,0,0,1,0,0 ,0
     &,0,0,0,1,0 ,0,0,0,0,0,1 /
      integer nedgePhicomp
      integer iedgePhilo0,iedgePhilo1
      integer iedgePhihi0,iedgePhihi1
      REAL*8 edgePhi(
     & iedgePhilo0:iedgePhihi0,
     & iedgePhilo1:iedgePhihi1,
     & 0:nedgePhicomp-1)
      integer ncellPhicomp
      integer icellPhilo0,icellPhilo1
      integer icellPhihi0,icellPhihi1
      REAL*8 cellPhi(
     & icellPhilo0:icellPhihi0,
     & icellPhilo1:icellPhihi1,
     & 0:ncellPhicomp-1)
      integer ifaceVello0,ifaceVello1
      integer ifaceVelhi0,ifaceVelhi1
      REAL*8 faceVel(
     & ifaceVello0:ifaceVelhi0,
     & ifaceVello1:ifaceVelhi1)
      integer iedgeBoxlo0,iedgeBoxlo1
      integer iedgeBoxhi0,iedgeBoxhi1
      integer idir
      integer n, ncell, i,j
      integer ii,jj
      REAL*8 val
      ii = 1 - CHF_ID(idir, 0)
      jj = 1 - CHF_ID(idir, 1)
      do n=0, (nedgePhicomp-1)
         if(nedgePhicomp.lt.ncellPhicomp) then
            ncell = idir
         else
            ncell = n
         endif
      do j = iedgeBoxlo1,iedgeBoxhi1
      do i = iedgeBoxlo0,iedgeBoxhi0
         if( faceVel(i,j).gt.(0.0d0) ) then
            val = cellPhi(i-ii,j-jj,ncell)
         else
            val = cellPhi(i,j,ncell)
         end if
            edgePhi(i,j,n) = val
      enddo
      enddo
      enddo
      return
      end
      subroutine C2FACE(
     & facePhi
     & ,ifacePhilo0,ifacePhilo1
     & ,ifacePhihi0,ifacePhihi1
     & ,nfacePhicomp
     & ,cellPhi
     & ,icellPhilo0,icellPhilo1
     & ,icellPhihi0,icellPhihi1
     & ,ncellPhicomp
     & ,ifaceBoxlo0,ifaceBoxlo1
     & ,ifaceBoxhi0,ifaceBoxhi1
     & ,idir
     & )
      implicit none
      integer*8 ch_flops
      COMMON/ch_timer/ ch_flops
      integer CHF_ID(0:5,0:5)
      data CHF_ID/ 1,0,0,0,0,0 ,0,1,0,0,0,0 ,0,0,1,0,0,0 ,0,0,0,1,0,0 ,0
     &,0,0,0,1,0 ,0,0,0,0,0,1 /
      integer nfacePhicomp
      integer ifacePhilo0,ifacePhilo1
      integer ifacePhihi0,ifacePhihi1
      REAL*8 facePhi(
     & ifacePhilo0:ifacePhihi0,
     & ifacePhilo1:ifacePhihi1,
     & 0:nfacePhicomp-1)
      integer ncellPhicomp
      integer icellPhilo0,icellPhilo1
      integer icellPhihi0,icellPhihi1
      REAL*8 cellPhi(
     & icellPhilo0:icellPhihi0,
     & icellPhilo1:icellPhihi1,
     & 0:ncellPhicomp-1)
      integer ifaceBoxlo0,ifaceBoxlo1
      integer ifaceBoxhi0,ifaceBoxhi1
      integer idir
      integer n, ncell, i,j
      integer ii,jj
      REAL*8 val
      ii = CHF_ID(idir, 0)
      jj = CHF_ID(idir, 1)
      do n=0, (nfacePhicomp-1)
        if(nfacePhicomp.lt.ncellPhicomp) then
           ncell = idir
        else
           ncell = n
        endif
      do j = ifaceBoxlo1,ifaceBoxhi1
      do i = ifaceBoxlo0,ifaceBoxhi0
          val = cellPhi(i-ii,j-jj,ncell)
     & + cellPhi(i,j,ncell)
        facePhi(i,j,n) = val * (0.500d0)
      enddo
      enddo
      enddo
      return
      end
      subroutine EXTRAP_FOR_CC_OPS(
     & dir
     & ,side
     & ,order
     & ,ifaceboxlo0,ifaceboxlo1
     & ,ifaceboxhi0,ifaceboxhi1
     & ,iinteriorboxlo0,iinteriorboxlo1
     & ,iinteriorboxhi0,iinteriorboxhi1
     & ,array
     & ,iarraylo0,iarraylo1
     & ,iarrayhi0,iarrayhi1
     & ,narraycomp
     & )
      implicit none
      integer*8 ch_flops
      COMMON/ch_timer/ ch_flops
      integer CHF_ID(0:5,0:5)
      data CHF_ID/ 1,0,0,0,0,0 ,0,1,0,0,0,0 ,0,0,1,0,0,0 ,0,0,0,1,0,0 ,0
     &,0,0,0,1,0 ,0,0,0,0,0,1 /
      integer dir
      integer side
      integer order
      integer ifaceboxlo0,ifaceboxlo1
      integer ifaceboxhi0,ifaceboxhi1
      integer iinteriorboxlo0,iinteriorboxlo1
      integer iinteriorboxhi0,iinteriorboxhi1
      integer narraycomp
      integer iarraylo0,iarraylo1
      integer iarrayhi0,iarrayhi1
      REAL*8 array(
     & iarraylo0:iarrayhi0,
     & iarraylo1:iarrayhi1,
     & 0:narraycomp-1)
      integer i,j
      integer id,jd
      integer ni,nj
      integer p, q, comp, ncomp
      double precision sum, coef2(0:2,2), coef4(0:4,3)
      data coef2 / 3.d0, -3.d0, 1.d0,
     & 6.d0, -8.d0, 3.d0 /
      data coef4 / 5.d0, -10.d0, 10.d0, -5.d0, 1.d0,
     & 15.d0, -40.d0, 45.d0, -24.d0, 5.d0,
     & 35.d0, -105.d0, 126.d0, -70.d0, 15.d0 /
      ncomp = narraycomp
      id = CHF_ID(0,dir)*side
      jd = CHF_ID(1,dir)*side
      do j = ifaceboxlo1,ifaceboxhi1
      do i = ifaceboxlo0,ifaceboxhi0
        if (side .eq. -1) then
           ni = id*(i-iinteriorboxlo0)
           q = ni
           nj = jd*(j-iinteriorboxlo1)
           q = q + nj
         else if (side .eq. 1) then
            ni = id*(i-iinteriorboxhi0)
            q = ni
            nj = jd*(j-iinteriorboxhi1)
            q = q + nj
          endif
          do comp = 0, ncomp-1
             sum = (0.0d0)
             if (order .eq. 4) then
                do p = 0, 4
                   sum = sum + coef4(p,q)*array(i-id*(ni+p),j-jd*(nj+p),
     &comp)
                enddo
                array(i,j,comp) = sum
             else if (order .eq. 2) then
                do p = 0, 2
                   sum = sum + coef2(p,q)*array(i-id*(ni+p),j-jd*(nj+p),
     &comp)
                enddo
                array(i,j,comp) = sum
             endif
          enddo
      enddo
      enddo
      return
      end
      subroutine EXTRAP_FOR_FC_OPS(
     & dir
     & ,side
     & ,order
     & ,ifaceboxlo0,ifaceboxlo1
     & ,ifaceboxhi0,ifaceboxhi1
     & ,iinteriorboxlo0,iinteriorboxlo1
     & ,iinteriorboxhi0,iinteriorboxhi1
     & ,array
     & ,iarraylo0,iarraylo1
     & ,iarrayhi0,iarrayhi1
     & ,narraycomp
     & )
      implicit none
      integer*8 ch_flops
      COMMON/ch_timer/ ch_flops
      integer CHF_ID(0:5,0:5)
      data CHF_ID/ 1,0,0,0,0,0 ,0,1,0,0,0,0 ,0,0,1,0,0,0 ,0,0,0,1,0,0 ,0
     &,0,0,0,1,0 ,0,0,0,0,0,1 /
      integer dir
      integer side
      integer order
      integer ifaceboxlo0,ifaceboxlo1
      integer ifaceboxhi0,ifaceboxhi1
      integer iinteriorboxlo0,iinteriorboxlo1
      integer iinteriorboxhi0,iinteriorboxhi1
      integer narraycomp
      integer iarraylo0,iarraylo1
      integer iarrayhi0,iarrayhi1
      REAL*8 array(
     & iarraylo0:iarrayhi0,
     & iarraylo1:iarrayhi1,
     & 0:narraycomp-1)
      integer i,j
      integer id,jd
      integer ni,nj
      integer p, q, comp, ncomp
      double precision sum, coef2(0:2,2), coef4(0:4,2)
      data coef2 / 3.d0, -3.d0, 1.d0,
     & 6.d0, -8.d0, 3.d0 /
      data coef4 / 4.625d0, -8.5d0, 7.75d0, -3.5d0, 0.625d0,
     & 11.25d0, -25.d0, 22.5d0, -9.d0, 1.25d0 /
      ncomp = narraycomp
      id = CHF_ID(0,dir)*side
      jd = CHF_ID(1,dir)*side
      do j = ifaceboxlo1,ifaceboxhi1
      do i = ifaceboxlo0,ifaceboxhi0
        if (side .eq. -1) then
           ni = id*(i-iinteriorboxlo0)
           q = ni
           nj = jd*(j-iinteriorboxlo1)
           q = q + nj
         else if (side .eq. 1) then
            ni = id*(i-iinteriorboxhi0)
            q = ni
            nj = jd*(j-iinteriorboxhi1)
            q = q + nj
          endif
          do comp = 0, ncomp-1
             sum = (0.0d0)
             if (order .eq. 4) then
                do p = 0, 4
                   sum = sum + coef4(p,q)*array(i-id*(ni+p),j-jd*(nj+p),
     &comp)
                enddo
                array(i,j,comp) = sum
             else if (order .eq. 2) then
                do p = 0, 2
                   sum = sum + coef2(p,q)*array(i-id*(ni+p),j-jd*(nj+p),
     &comp)
                enddo
                array(i,j,comp) = sum
             endif
          enddo
      enddo
      enddo
      return
      end
      subroutine FACE_CENTERED_GRAD_COMPONENT(
     & iboxlo0,iboxlo1
     & ,iboxhi0,iboxhi1
     & ,dir
     & ,var
     & ,ivarlo0,ivarlo1
     & ,ivarhi0,ivarhi1
     & ,h
     & ,order
     & ,grad
     & ,igradlo0,igradlo1
     & ,igradhi0,igradhi1
     & )
      implicit none
      integer*8 ch_flops
      COMMON/ch_timer/ ch_flops
      integer CHF_ID(0:5,0:5)
      data CHF_ID/ 1,0,0,0,0,0 ,0,1,0,0,0,0 ,0,0,1,0,0,0 ,0,0,0,1,0,0 ,0
     &,0,0,0,1,0 ,0,0,0,0,0,1 /
      integer iboxlo0,iboxlo1
      integer iboxhi0,iboxhi1
      integer dir
      integer ivarlo0,ivarlo1
      integer ivarhi0,ivarhi1
      REAL*8 var(
     & ivarlo0:ivarhi0,
     & ivarlo1:ivarhi1)
      REAL*8 h(0:1)
      integer order
      integer igradlo0,igradlo1
      integer igradhi0,igradhi1
      REAL*8 grad(
     & igradlo0:igradhi0,
     & igradlo1:igradhi1)
      integer i,j
      integer ii,jj
      ii = CHF_ID(0,dir)
      jj = CHF_ID(1,dir)
      if (order .eq. 4) then
      do j = iboxlo1,iboxhi1
      do i = iboxlo0,iboxhi0
            grad(i,j) = (
     & 27.d0 * (var(i ,j )
     & - var(i- ii,j- jj))
     & - var(i+ ii,j+ jj)
     & + var(i-2*ii,j-2*jj)
     & ) / (24.d0 * h(dir))
      enddo
      enddo
      else if (order .eq. 2) then
      do j = iboxlo1,iboxhi1
      do i = iboxlo0,iboxhi0
            grad(i,j) = (
     & - var(i-ii,j-jj)
     & + var(i ,j )
     & ) / h(dir)
      enddo
      enddo
      endif
      return
      end
      subroutine CELL_CENTERED_GRAD_COMPONENT(
     & iboxlo0,iboxlo1
     & ,iboxhi0,iboxhi1
     & ,dir
     & ,var
     & ,ivarlo0,ivarlo1
     & ,ivarhi0,ivarhi1
     & ,h
     & ,order
     & ,grad
     & ,igradlo0,igradlo1
     & ,igradhi0,igradhi1
     & )
      implicit none
      integer*8 ch_flops
      COMMON/ch_timer/ ch_flops
      integer CHF_ID(0:5,0:5)
      data CHF_ID/ 1,0,0,0,0,0 ,0,1,0,0,0,0 ,0,0,1,0,0,0 ,0,0,0,1,0,0 ,0
     &,0,0,0,1,0 ,0,0,0,0,0,1 /
      integer iboxlo0,iboxlo1
      integer iboxhi0,iboxhi1
      integer dir
      integer ivarlo0,ivarlo1
      integer ivarhi0,ivarhi1
      REAL*8 var(
     & ivarlo0:ivarhi0,
     & ivarlo1:ivarhi1)
      REAL*8 h(0:1)
      integer order
      integer igradlo0,igradlo1
      integer igradhi0,igradhi1
      REAL*8 grad(
     & igradlo0:igradhi0,
     & igradlo1:igradhi1)
      integer i,j
      integer ii,jj
      ii = CHF_ID(0,dir)
      jj = CHF_ID(1,dir)
      if (order .eq. 4) then
      do j = iboxlo1,iboxhi1
      do i = iboxlo0,iboxhi0
            grad(i,j) = (
     & var(i-2*ii,j-2*jj)
     & - (8.0d0) * var(i-ii,j-jj)
     & + (8.0d0) * var(i+ii,j+jj)
     & - var(i+2*ii,j+2*jj)
     & ) / ((12.0d0) * h(dir))
      enddo
      enddo
      else if (order .eq. 2) then
      do j = iboxlo1,iboxhi1
      do i = iboxlo0,iboxhi0
            grad(i,j) = (
     & - var(i-ii,j-jj)
     & + var(i+ii,j+jj)
     & ) / ((2.0d0) * h(dir))
      enddo
      enddo
      endif
      return
      end
      subroutine FACE_INTERPOLATE(
     & dir
     & ,iboxlo0,iboxlo1
     & ,iboxhi0,iboxhi1
     & ,order
     & ,var
     & ,ivarlo0,ivarlo1
     & ,ivarhi0,ivarhi1
     & ,face_var
     & ,iface_varlo0,iface_varlo1
     & ,iface_varhi0,iface_varhi1
     & )
      implicit none
      integer*8 ch_flops
      COMMON/ch_timer/ ch_flops
      integer CHF_ID(0:5,0:5)
      data CHF_ID/ 1,0,0,0,0,0 ,0,1,0,0,0,0 ,0,0,1,0,0,0 ,0,0,0,1,0,0 ,0
     &,0,0,0,1,0 ,0,0,0,0,0,1 /
      integer dir
      integer iboxlo0,iboxlo1
      integer iboxhi0,iboxhi1
      integer order
      integer ivarlo0,ivarlo1
      integer ivarhi0,ivarhi1
      REAL*8 var(
     & ivarlo0:ivarhi0,
     & ivarlo1:ivarhi1)
      integer iface_varlo0,iface_varlo1
      integer iface_varhi0,iface_varhi1
      REAL*8 face_var(
     & iface_varlo0:iface_varhi0,
     & iface_varlo1:iface_varhi1)
      integer i,j
      integer ii,jj
      ii = CHF_ID(0,dir)
      jj = CHF_ID(1,dir)
      if (order .eq. 4) then
      do j = iboxlo1,iboxhi1
      do i = iboxlo0,iboxhi0
            face_var(i,j) = (
     & 9.d0 * (var(i ,j )
     & + var(i- ii,j- jj))
     & - (var(i+ ii,j+ jj)
     & + var(i-2*ii,j-2*jj))
     & ) / 16.d0
      enddo
      enddo
      else if (order .eq. 2) then
      do j = iboxlo1,iboxhi1
      do i = iboxlo0,iboxhi0
         face_var(i,j) = (
     & var(i-ii,j-jj)
     & + var(i ,j )
     & ) / 2.d0
      enddo
      enddo
      endif
      return
      end
      subroutine SECOND_ORDER_EXTRAPOLATION(
     & dir
     & ,side
     & ,isrcboxlo0,isrcboxlo1
     & ,isrcboxhi0,isrcboxhi1
     & ,idstboxlo0,idstboxlo1
     & ,idstboxhi0,idstboxhi1
     & ,array
     & ,iarraylo0,iarraylo1
     & ,iarrayhi0,iarrayhi1
     & ,narraycomp
     & )
      implicit none
      integer*8 ch_flops
      COMMON/ch_timer/ ch_flops
      integer CHF_ID(0:5,0:5)
      data CHF_ID/ 1,0,0,0,0,0 ,0,1,0,0,0,0 ,0,0,1,0,0,0 ,0,0,0,1,0,0 ,0
     &,0,0,0,1,0 ,0,0,0,0,0,1 /
      integer dir
      integer side
      integer isrcboxlo0,isrcboxlo1
      integer isrcboxhi0,isrcboxhi1
      integer idstboxlo0,idstboxlo1
      integer idstboxhi0,idstboxhi1
      integer narraycomp
      integer iarraylo0,iarraylo1
      integer iarrayhi0,iarrayhi1
      REAL*8 array(
     & iarraylo0:iarrayhi0,
     & iarraylo1:iarrayhi1,
     & 0:narraycomp-1)
      double precision coef(0:2), sum
      data coef / 3.d0, -3.d0, 1.d0 /
      integer i,id,ni,j,jd,nj, p, comp, ncomp
      ni = 0
      nj = 0
      ncomp = narraycomp
      id = CHF_ID(0,dir)*side
      jd = CHF_ID(1,dir)*side
      do j = idstboxlo1,idstboxhi1
      do i = idstboxlo0,idstboxhi0
        if (side .eq. -1) then
           ni = id*(i-isrcboxlo0)
           nj = jd*(j-isrcboxlo1)
        else if (side .eq. 1) then
           ni = id*(i-isrcboxhi0)
           nj = jd*(j-isrcboxhi1)
          endif
          do comp = 0, ncomp-1
             sum = 0.d0
             do p = 0, 2
                sum = sum + coef(p)*
     & array(i-id*(ni+p),j-jd*(nj+p),comp)
             enddo
             array(i,j,comp) = sum
          enddo
      enddo
      enddo
      return
      end
      subroutine COPY(
     & igridboxlo0,igridboxlo1
     & ,igridboxhi0,igridboxhi1
     & ,dst
     & ,idstlo0,idstlo1
     & ,idsthi0,idsthi1
     & ,ndstcomp
     & ,src
     & ,isrclo0,isrclo1
     & ,isrchi0,isrchi1
     & ,nsrccomp
     & )
      implicit none
      integer*8 ch_flops
      COMMON/ch_timer/ ch_flops
      integer igridboxlo0,igridboxlo1
      integer igridboxhi0,igridboxhi1
      integer ndstcomp
      integer idstlo0,idstlo1
      integer idsthi0,idsthi1
      REAL*8 dst(
     & idstlo0:idsthi0,
     & idstlo1:idsthi1,
     & 0:ndstcomp-1)
      integer nsrccomp
      integer isrclo0,isrclo1
      integer isrchi0,isrchi1
      REAL*8 src(
     & isrclo0:isrchi0,
     & isrclo1:isrchi1,
     & 0:nsrccomp-1)
      integer i,j, comp
      do j = igridboxlo1,igridboxhi1
      do i = igridboxlo0,igridboxhi0
         do comp = 0, ndstcomp-1
           dst(i,j,comp) = src(i,j,comp)
         enddo
      enddo
      enddo
      return
      end
      subroutine VECTOR_NORM(
     & igridboxlo0,igridboxlo1
     & ,igridboxhi0,igridboxhi1
     & ,dst
     & ,idstlo0,idstlo1
     & ,idsthi0,idsthi1
     & ,src
     & ,isrclo0,isrclo1
     & ,isrchi0,isrchi1
     & ,nsrccomp
     & )
      implicit none
      integer*8 ch_flops
      COMMON/ch_timer/ ch_flops
      integer igridboxlo0,igridboxlo1
      integer igridboxhi0,igridboxhi1
      integer idstlo0,idstlo1
      integer idsthi0,idsthi1
      REAL*8 dst(
     & idstlo0:idsthi0,
     & idstlo1:idsthi1)
      integer nsrccomp
      integer isrclo0,isrclo1
      integer isrchi0,isrchi1
      REAL*8 src(
     & isrclo0:isrchi0,
     & isrclo1:isrchi1,
     & 0:nsrccomp-1)
      integer i,j, comp
      double precision val
      do j = igridboxlo1,igridboxhi1
      do i = igridboxlo0,igridboxhi0
         val = 0.0
         do comp = 0, nsrccomp-1
           val = val + src(i,j,comp)*src(i,j,comp)
         enddo
         dst(i,j) = sqrt(val)
      enddo
      enddo
      return
      end
      subroutine INSPECT_FARRAYBOX(
     & igridboxlo0,igridboxlo1
     & ,igridboxhi0,igridboxhi1
     & ,FA0
     & ,iFA0lo0,iFA0lo1
     & ,iFA0hi0,iFA0hi1
     & )
      implicit none
      integer*8 ch_flops
      COMMON/ch_timer/ ch_flops
      integer igridboxlo0,igridboxlo1
      integer igridboxhi0,igridboxhi1
      integer iFA0lo0,iFA0lo1
      integer iFA0hi0,iFA0hi1
      REAL*8 FA0(
     & iFA0lo0:iFA0hi0,
     & iFA0lo1:iFA0hi1)
      integer i0,i1
      double precision F00
      do i1 = igridboxlo1,igridboxhi1
      do i0 = igridboxlo0,igridboxhi0
        F00 = FA0(i0,i1)
        print*, "i0,i1    = ", i0,i1
        print*, "F00    = ", F00
      enddo
      enddo
      return
      end
      subroutine INSPECT_FLUXBOX(
     & igridboxlo0,igridboxlo1
     & ,igridboxhi0,igridboxhi1
     & ,Flux
     & ,iFluxlo0,iFluxlo1
     & ,iFluxhi0,iFluxhi1
     & ,nFluxcomp
     & ,dir
     & )
      implicit none
      integer*8 ch_flops
      COMMON/ch_timer/ ch_flops
      integer CHF_ID(0:5,0:5)
      data CHF_ID/ 1,0,0,0,0,0 ,0,1,0,0,0,0 ,0,0,1,0,0,0 ,0,0,0,1,0,0 ,0
     &,0,0,0,1,0 ,0,0,0,0,0,1 /
      integer igridboxlo0,igridboxlo1
      integer igridboxhi0,igridboxhi1
      integer nFluxcomp
      integer iFluxlo0,iFluxlo1
      integer iFluxhi0,iFluxhi1
      REAL*8 Flux(
     & iFluxlo0:iFluxhi0,
     & iFluxlo1:iFluxhi1,
     & 0:nFluxcomp-1)
      integer dir
      integer n, i0,i1
      integer ii,jj
      double precision F00, F01, F10, F11, F2, F3
      integer n0, n1
      ii = CHF_ID(dir, 0)
      jj = CHF_ID(dir, 1)
      do n=0, (nFluxcomp-1)
        print*, "component    = ", n
      do i1 = igridboxlo1,igridboxhi1
      do i0 = igridboxlo0,igridboxhi0
        print*, "i0,i1  = ", i0,i1
        print*, "Flux    = ", Flux(i0,i1,n)
      enddo
      enddo
      enddo
      return
      end
