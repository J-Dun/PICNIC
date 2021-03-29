      subroutine CELLTOEDGE(
     & cellData
     & ,icellDatalo0,icellDatalo1
     & ,icellDatahi0,icellDatahi1
     & ,edgeData
     & ,iedgeDatalo0,iedgeDatalo1
     & ,iedgeDatahi0,iedgeDatahi1
     & ,iedgeBoxlo0,iedgeBoxlo1
     & ,iedgeBoxhi0,iedgeBoxhi1
     & ,dir
     & )
      implicit none
      integer*8 ch_flops
      COMMON/ch_timer/ ch_flops
      integer CHF_ID(0:5,0:5)
      data CHF_ID/ 1,0,0,0,0,0 ,0,1,0,0,0,0 ,0,0,1,0,0,0 ,0,0,0,1,0,0 ,0
     &,0,0,0,1,0 ,0,0,0,0,0,1 /
      integer icellDatalo0,icellDatalo1
      integer icellDatahi0,icellDatahi1
      REAL*8 cellData(
     & icellDatalo0:icellDatahi0,
     & icellDatalo1:icellDatahi1)
      integer iedgeDatalo0,iedgeDatalo1
      integer iedgeDatahi0,iedgeDatahi1
      REAL*8 edgeData(
     & iedgeDatalo0:iedgeDatahi0,
     & iedgeDatalo1:iedgeDatahi1)
      integer iedgeBoxlo0,iedgeBoxlo1
      integer iedgeBoxhi0,iedgeBoxhi1
      integer dir
      integer i,j
      integer ii,jj
      do j = iedgeBoxlo1,iedgeBoxhi1
      do i = iedgeBoxlo0,iedgeBoxhi0
        ii = i-CHF_ID(0,dir)
        jj = j-CHF_ID(1,dir)
        edgeData(i,j) = (0.500d0)*(
     & cellData(ii,jj)
     & + cellData(i,j) )
      enddo
      enddo
        return
        end
