/*
  This source is part of the libosmscout library
  Copyright (C) 2016  Tim Teulings

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
*/

#include <osmscout/import/GenCoordDat.h>

#include <limits>

#include <osmscout/Coord.h>
#include <osmscout/CoordDataFile.h>

#include <osmscout/import/Preprocess.h>
#include <osmscout/import/RawCoord.h>

#include <osmscout/util/String.h>

namespace osmscout {

  static uint32_t coordPageSize=5000000;
  static uint32_t coordBlockSize=60000000;

  static inline bool SortCoordsByCoordId(const Id& a, const Id& b)
  {
    return a<b;
  }

  static inline bool SortCoordsByOSMId(const RawCoord& a, const RawCoord& b)
  {
    return a.GetOSMId()<b.GetOSMId();
  }

  CoordDataGenerator::CoordDataGenerator()
  {
    // no code
  }

  bool CoordDataGenerator::FindDuplicateCoordinates(const TypeConfig& typeConfig,
                                                    const ImportParameter& parameter,
                                                    Progress& progress,
                                                    std::unordered_map<Id,uint8_t>& duplicates) const
  {
    progress.SetAction("Searching for duplicate coordinates");

    Id          maxId=GeoCoord(90.0,180.0).GetId();
    Id          currentLowerLimit=0;
    Id          currentUpperLimit=maxId/coordPageSize;
    FileScanner scanner;
    uint32_t    loadedCoordCount=0;

    try {
      scanner.Open(AppendFileToDir(parameter.GetDestinationDirectory(),
                                   Preprocess::RAWCOORDS_DAT),
                   FileScanner::Sequential,
                   true);

      scanner.GotoBegin();

      uint32_t coordCount;

      scanner.Read(coordCount);

      while (loadedCoordCount<coordCount) {
        std::map<Id,std::vector<Id>> coordPages;
        uint32_t                     currentCoordCount=0;

        progress.Info("Searching for coordinates with page id >= "+NumberToString(currentLowerLimit));

        scanner.GotoBegin();

        scanner.Read(coordCount);

        RawCoord coord;

        for (uint32_t i=1; i<=coordCount; i++) {
          progress.SetProgress(i,coordCount);

          coord.Read(typeConfig,scanner);

          Id id=coord.GetCoord().GetId();
          Id pageId=id/coordPageSize;

          if (pageId<currentLowerLimit || pageId>currentUpperLimit) {
            continue;
          }

          coordPages[pageId].push_back(id);
          currentCoordCount++;
          loadedCoordCount++;

          if (loadedCoordCount==coordCount) {
            break;
          }

          if (currentCoordCount>coordBlockSize
              && coordPages.size()>1) {
            Id oldUpperLimit=currentUpperLimit;

            while (currentCoordCount>coordBlockSize
                   && coordPages.size()>1) {
              auto pageEntry=coordPages.rbegin();
              Id highestPageId=pageEntry->first;

              currentCoordCount-=pageEntry->second.size();
              loadedCoordCount-=pageEntry->second.size();
              coordPages.erase(highestPageId);
              currentUpperLimit=highestPageId-1;
            }

            assert(currentUpperLimit<oldUpperLimit);
          }
        }

        progress.Info("Sorting coordinates");

        // TODO: Sort in parallel, no side effect!
        for (auto& entry : coordPages) {
          std::sort(entry.second.begin(),
                    entry.second.end(),
                    SortCoordsByCoordId);
        }

        for (auto& entry : coordPages) {
          Id lastId=std::numeric_limits<Id>::max();

          bool flaged=false;
          for (auto& id : entry.second) {
            if (id==lastId) {
              if (!flaged) {
                duplicates[id]=1;
                flaged=true;
              }
            }
            else {
              flaged=false;
            }

            lastId=id;
          }
        }

        progress.Info("Loaded "+NumberToString(currentCoordCount)+" coords (" +NumberToString(loadedCoordCount)+"/"+NumberToString(coordCount)+")");

        currentLowerLimit=currentUpperLimit+1;
        currentUpperLimit=maxId/coordPageSize;
      }

      progress.Info("Found "+NumberToString(duplicates.size())+" duplicate cordinates");

      scanner.Close();
    }
    catch (IOException& e) {
      progress.Error(e.GetDescription());
      scanner.CloseFailsafe();

      return false;
    }

    return true;
  }


  bool CoordDataGenerator::StoreCoordinates(const TypeConfig& typeConfig,
                                            const ImportParameter& parameter,
                                            Progress& progress,
                                            std::unordered_map<Id,uint8_t>& duplicates) const
  {
    progress.SetAction("Storing coordinates");

    OSMId       maxId=std::numeric_limits<OSMId>::max();
    OSMId       currentLowerLimit=std::numeric_limits<OSMId>::min()/coordPageSize;
    OSMId       currentUpperLimit=maxId/coordPageSize;
    FileScanner scanner;
    FileWriter  writer;
    uint32_t    loadedCoordCount=0;

    try {
      writer.Open(AppendFileToDir(parameter.GetDestinationDirectory(),
                                  CoordDataFile::COORD_DAT));

      writer.Write(loadedCoordCount);

      scanner.Open(AppendFileToDir(parameter.GetDestinationDirectory(),
                                   Preprocess::RAWCOORDS_DAT),
                   FileScanner::Sequential,
                   true);

      scanner.GotoBegin();

      uint32_t coordCount;

      scanner.Read(coordCount);

      while (loadedCoordCount<coordCount) {
        std::map<Id,std::vector<RawCoord>> coordPages;
        uint32_t                           currentCoordCount=0;

        progress.Info("Search for coordinates with page id >= "+NumberToString(currentLowerLimit));

        scanner.GotoBegin();

        scanner.Read(coordCount);

        RawCoord coord;

        for (uint32_t i=1; i<=coordCount; i++) {
          progress.SetProgress(i,coordCount);

          coord.Read(typeConfig,scanner);

          OSMId id=coord.GetOSMId();
          OSMId pageId=id/coordPageSize;

          if (pageId<currentLowerLimit || pageId>currentUpperLimit) {
            continue;
          }

          coordPages[pageId].push_back(coord);
          currentCoordCount++;
          loadedCoordCount++;

          if (loadedCoordCount==coordCount) {
            break;
          }

          if (currentCoordCount>coordBlockSize
              && coordPages.size()>1) {
            OSMId oldUpperLimit=currentUpperLimit;

            while (currentCoordCount>coordBlockSize
                   && coordPages.size()>1) {
              auto pageEntry=coordPages.rbegin();
              OSMId highestPageId=pageEntry->first;

              currentCoordCount-=pageEntry->second.size();
              loadedCoordCount-=pageEntry->second.size();
              coordPages.erase(highestPageId);
              currentUpperLimit=highestPageId-1;
            }

            assert(currentUpperLimit<oldUpperLimit);
          }
        }

        progress.Info("Sorting coordinates");

        // TODO: Sort in parallel, no side effect!
        for (auto& entry : coordPages) {
          std::sort(entry.second.begin(),
                    entry.second.end(),
                    SortCoordsByOSMId);
        }

        for (auto& entry : coordPages) {
          for (auto& osmCoord : entry.second) {
            uint8_t serial=1;
            auto    duplicateEntry=duplicates.find(osmCoord.GetCoord().GetId());

            if (duplicateEntry!=duplicates.end()) {
              serial=duplicateEntry->second;

              if (serial==255) {
                progress.Error("Coordinate " + osmCoord.GetCoord().GetDisplayText()+" has more than 256 nodes");
                return false;
              }

              duplicateEntry->second++;
            }

            Coord coord(osmCoord.GetOSMId(),
                        serial,
                        osmCoord.GetCoord());

            coord.Write(typeConfig,writer);
          }
        }

        progress.Info("Loaded "+NumberToString(currentCoordCount)+" coords (" +NumberToString(loadedCoordCount)+"/"+NumberToString(coordCount)+")");

        currentLowerLimit=currentUpperLimit+1;
        currentUpperLimit=maxId/coordPageSize;
      }

      scanner.Close();

      writer.GotoBegin();
      writer.Write(loadedCoordCount);
      writer.Close();
    }
    catch (IOException& e) {
      progress.Error(e.GetDescription());
      scanner.CloseFailsafe();
      writer.CloseFailsafe();

      return false;
    }

    return true;
  }

  void CoordDataGenerator::GetDescription(const ImportParameter& /*parameter*/,
                                          ImportModuleDescription& description) const
  {
    description.SetName("CoordDataGenerator");
    description.SetDescription("Generate coord data file");

    description.AddRequiredFile(Preprocess::RAWCOORDS_DAT);

    description.AddProvidedDebuggingFile(CoordDataFile::COORD_DAT);
  }

  bool CoordDataGenerator::Import(const TypeConfigRef& typeConfig,
                                  const ImportParameter& parameter,
                                  Progress& progress)
  {
    std::unordered_map<Id,uint8_t> duplicates;

    if (!FindDuplicateCoordinates(*typeConfig,
                                  parameter,
                                  progress,
                                  duplicates)) {
      return false;
    }

    if (!StoreCoordinates(*typeConfig,
                          parameter,
                          progress,
                          duplicates)) {
      return false;
    }

    return true;
  }
}

                                   ;