function obj= imagestrace(obj)
global self;
global objects;
global db;
pathnames=obj.state.display.pathnames;
pathname=obj.state.pathname;
fileselection=obj.state.display.fileselection;
[dbname, dbpath] =  uigetfile({'*.db'}, 'Choose database file to load');
load('-mat',[dbpath '\' dbname]);

for z=1:size(pathnames,2)
    try    
        [path,name,ext] = fileparts([char(pathnames(z))]);
        [name,status]=strtok(name,'_');
        if (strcmp(ext,'.spi'))
            warning off MATLAB:MKDIR:DirectoryExists;
            mkdir(path, [name 'ziptemp']);
            h = waitbar(0,'Unzipping Spi File...', 'Name', 'Unzipping Spi File', 'Pointer', 'watch');
            waitbar(0,h, ['Unzipping Spi File...']);
            if exist([path '\' name 'ziptemp'],'dir')
                tiffile=dir([path '\' name 'ziptemp\*.tif']);
                filtif=dir([path '\' name 'ziptemp\*.tif']);
                datfile=dir([path '\' name 'ziptemp\*.dat']);
                if ~((size(tiffile,1)>=2) & (size(filtif,1)>=1) & (size(datfile,1)>=1))
                    % infounzip([path '\' name ext],[path '\' name 'ziptemp']);
                    [s,w]=system(['unzip -u ' path '\' name status ext ' -d ' path '\' name 'ziptemp']);
                    if (s~=0)
                        disp('Could not unzip data');
                        close(h);
                        return;
                    end
                end
            else
                %  infounzip([path '\' name ext],[path '\' name 'ziptemp']);
                [s,w]=system(['unzip -u ' path '\' name status ext ' -d ' path '\' name 'ziptemp']);
                if (s~=0)
                    disp('Could not unzip data');
                    close(h);
                    return;
                end
            end
            try
                cd ([path '\' name 'ziptemp\ziptemp']);
                copyfile('*.*','..');
            catch
                
            end
            waitbar(1,h, 'Done');
            close(h);
            
            
            cd (path);
            
            
            datfile=dir([path '\' name 'ziptemp\*.dat']);
            load('-mat',[path '\' name 'ziptemp\' datfile(1).name]);
            
            
            tiffile=dir([path '\' name 'ziptemp\*.tif']);
            try
                self.data.imageArray=opentif([path '\' name 'ziptemp\' tiffile(1).name]);  % get the image
            catch
                disp('warning: no rawImage found');
            end
            
            %get raw image
            
            % Add experiment
            if (size(db.experiments,2)==0)
                uid=1;
            else
                uid=max([db.experiments.uid])+1;
            end
            db.experiments(uid).uid=uid;
            db.experiments(uid).imagemedian=self.data.imagemedian;
            db.experiments(uid).rawImagePath=self.state.rawImagePath;
            db.experiments(uid).rawImageName=self.state.rawImageName;
            db.experiments(uid).parameters=self.parameters;
            db.experiments(uid).sereisuid=0;
            experimentid=uid;
           
            
            % Add dendrites
            for i=1:size(self.data.dendrites,2)
                if (size(db.dendrites,2)==0)
                    uid=1;
                else
                    uid=max([db.dendrites.uid])+1;
                end
                pos=size(db.dendrites,2)+1;
                names=fieldnames(self.data.dendrites(i));
                for j=1:size(names,1)
                    db.dendrites(pos).(char(names(j)))=self.data.dendrites(i).(char(names(j)));
                end
%                [maxint,voxelmax]=getbackboneintensity(double(voxel),obj.data.imageArray,dx,dy,3,1,0);       
                [db.dendrites(pos).maxintcube,voxelmax,db.dendrites(pos).cube]=getbackboneintensity(double(db.dendrites(pos).voxel),self.data.imageArray,db.dendrites(pos).dx,db.dendrites(pos).dy,20,1,1);
                db.dendrites(pos).widthprofile=mean(db.dendrites(pos).cube(:,:,2),2);    
                db.dendrites(pos).uid=uid;
                self.data.dendrites(i).uid=uid;
                db.dendrites(pos).seriesuid=0;
                db.dendrites(pos).experimentuid=experimentid;
                db.dendrites(pos).totallength=max(db.dendrites(uid).length);
                db.dendrites(pos).spineuids=[];
                db.dendrites(pos).spineints=[];
            end
            % Add spines
            
            for i=1:size(self.data.spines,2)
                if isempty(db.spines)
                    uid=1;
                else
                    uid=max([db.spines.uid])+1;
                end
                pos=size(db.spines,2)+1;
                db.spines(pos).uid=uid;
                self.data.spines(i).uid=uid;
                den_uid=self.data.dendrites(self.data.spines(i).den_ind).uid;
                db.spines(uid).den_uid=den_uid;
                if (self.data.spines(i).len==0)
                    db.spines(pos).intensity=db.dendrites([db.dendrites.uid]==den_uid).maxint(self.data.spines(i).dendis);
                    db.spines(pos).intensitycube=db.dendrites([db.dendrites.uid]==den_uid).maxintcube(self.data.spines(i).dendis);
                else
                    db.spines(pos).intensity=-1;
                    %self.data.filtereddata(db.spines(uid).voxel(1,1),db.spines(uid).voxel(2,1),db.spines(uid).voxel(3,1));
                    %     db.spines(uid).intensity=self.data.filtereddata(db.spines(uid).voxel(1,1),db.spines(uid).voxel(2,1),db.spines(uid).voxel(3,1))*db.dendrites(den_uid).meanback/db.dendrites(den_uid).maxint(db.spines(uid).den_ind);
                end
                names=fieldnames(self.data.spines(i));
                for j=1:size(names,1)
                    db.spines(pos).(char(names(j)))=self.data.spines(i).(char(names(j)));
                end
                db.spines(pos).uid=uid;
                db.dendrites([db.dendrites.uid]==den_uid).spineuids=[db.dendrites([db.dendrites.uid]==den_uid).spineuids uid];
                db.dendrites([db.dendrites.uid]==den_uid).spineints=[db.dendrites([db.dendrites.uid]==den_uid).spineints db.spines(pos).intensity];
                db.spines(pos).seriesuid=0;
                db.spines(pos).experimentuid=experimentid;
            end
            for i=1:size(self.data.dendrites,2)
                uid=self.data.dendrites(i).uid;
                %recalculate enpassant boutons
                db.dendrites([db.dendrites.uid]==uid).positions=findTerminals(double(db.dendrites([db.dendrites.uid]==uid).maxint)./median(double(db.dendrites([db.dendrites.uid]==uid).maxint)), 0.5, 1.5, 35,10);
                db.dendrites([db.dendrites.uid]==uid).intensities=double(db.dendrites([db.dendrites.uid]==uid).maxint(db.dendrites([db.dendrites.uid]==uid).positions)')./double(db.dendrites([db.dendrites.uid]==uid).meanback);
                db.dendrites([db.dendrites.uid]==uid).cubeintensities=double(db.dendrites([db.dendrites.uid]==uid).maxintcube(db.dendrites([db.dendrites.uid]==uid).positions)')./median(double(db.dendrites([db.dendrites.uid]==uid).maxintcube));               
                db.dendrites([db.dendrites.uid]==uid).maxintensities=max(db.dendrites([db.dendrites.uid]==uid).intensities);
                db.dendrites([db.dendrites.uid]==uid).maxcubeintensities=max(db.dendrites([db.dendrites.uid]==uid).cubeintensities);
                db.dendrites([db.dendrites.uid]==uid).meanintensities=mean(db.dendrites([db.dendrites.uid]==uid).intensities);
                db.dendrites([db.dendrites.uid]==uid).terminalpositions=[db.spines( ([db.spines.den_uid]==uid) & ([db.spines.len]>0) ).dendis];
                db.dendrites([db.dendrites.uid]==uid).terminalintensities=double([db.spines(([db.spines.den_uid]==uid) & ([db.spines.len]>0) ).dendis])./double(db.dendrites([db.dendrites.uid]==uid).meanback);
                db.dendrites([db.dendrites.uid]==uid).terminallengths=[db.spines(([db.spines.den_uid]==uid) & ([db.spines.len]>0) ).len];
                db.dendrites([db.dendrites.uid]==uid).maxterminalintensities=max(db.dendrites([db.dendrites.uid]==uid).terminalintensities);
                db.dendrites([db.dendrites.uid]==uid).meanterminalintensities=mean(db.dendrites([db.dendrites.uid]==uid).terminalintensities);
                db.dendrites([db.dendrites.uid]==uid).ibs=db.dendrites([db.dendrites.uid]==uid).totallength/size(db.dendrites([db.dendrites.uid]==uid).positions,2);
                db.dendrites([db.dendrites.uid]==uid).itbs=db.dendrites([db.dendrites.uid]==uid).totallength/size(db.dendrites([db.dendrites.uid]==uid).terminalpositions,2);
                db.dendrites([db.dendrites.uid]==uid).variability=std(db.dendrites([db.dendrites.uid]==uid).medianfiltered);
                db.dendrites([db.dendrites.uid]==uid).name=name;
            end
        end     
    catch
        disp(['error processing:' name]);
        disp(lasterr);
    end
end
button = questdlg('save to database?');
if (button=='Yes')
    save('-mat',[dbpath dbname],'db');
end
