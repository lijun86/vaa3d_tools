function obj = cutStack(obj)
if (obj.state.display.showprojection==1) && isfield(obj.gh.projectionGUI,'Figure') && obj.gh.projectionGUI.Figure 
    [xv,yv]=getline(get(obj.gh.projectionGUI.Figure,'Children'),'closed');
    if ((obj.state.display.showprojection==1) && isfield(obj.data,'filteredArray') && size(obj.data.filteredArray,2)>0) || ((obj.state.display.showprojection==1) && isfield(obj.data,'imageArray') && size(obj.data.imageArray,2)>0)
        maxx=size(obj.data.filteredArray,1);
        maxy=size(obj.data.filteredArray,2);
        allpointsy=ones(maxx,1)*(1:maxy);
        allpointsx=(1:maxx)'*ones(maxy,1)';
        in=inpolygon(allpointsy(:),allpointsx(:),xv,yv);
        ind=find(in);
        
        for z=obj.state.ROI.minz:obj.state.ROI.maxz
            obj.data.filteredArray(ind+(z-1)*maxx*maxy)=0;
            obj.data.imageArray(ind+(z-1)*maxx*maxy)=0;
        end
    end
end