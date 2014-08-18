#ifndef OSMSCOUT_AREA_H
#define OSMSCOUT_AREA_H

/*
  This source is part of the libosmscout library
  Copyright (C) 2013  Tim Teulings

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

#include <osmscout/GeoCoord.h>

#include <osmscout/AttributeAccess.h>
#include <osmscout/TypeConfig.h>

#include <osmscout/util/FileScanner.h>
#include <osmscout/util/FileWriter.h>
#include <osmscout/util/Progress.h>
#include <osmscout/util/Reference.h>

namespace osmscout {
  /**
    Representation of an (complex/multipolygon) area
    */
  class OSMSCOUT_API Area : public Referencable
  {
  public:
    static const size_t masterRingId = 0;
    static const size_t outerRingId = 1;

  public:
    class Ring
    {
    public:
      FeatureValueBuffer    featureValueBuffer; //! List of features
      uint8_t               ring;               //! The ring hierarchy number (0...n)
      std::vector<Id>       ids;                //! The array of ids for a coordinate
      std::vector<GeoCoord> nodes;              //! The array of coordinates

    public:
      inline Ring()
      : ring(0)
      {
        // no code
      }

      inline TypeInfoRef GetType() const
      {
        return featureValueBuffer.GetType();
      }

      inline TypeId GetTypeId() const
      {
        return featureValueBuffer.GetTypeId();
      }

      inline size_t GetFeatureCount() const
      {
        return featureValueBuffer.GetType()->GetFeatureCount();
      }

      inline bool HasFeature(size_t idx) const
      {
        return featureValueBuffer.HasValue(idx);
      }

      inline FeatureInstance GetFeature(size_t idx) const
      {
        return featureValueBuffer.GetType()->GetFeature(idx);
      }

      inline FeatureValue* GetFeatureValue(size_t idx) const
      {
        return featureValueBuffer.GetValue(idx);
      }

      inline void UnsetFeature(size_t idx)
      {
        featureValueBuffer.FreeValue(idx);
      }

      inline const FeatureValueBuffer& GetFeatureValueBuffer() const
      {
        return featureValueBuffer;
      }

      bool GetCenter(double& lat,
                     double& lon) const;

      void GetBoundingBox(double& minLon,
                          double& maxLon,
                          double& minLat,
                          double& maxLat) const;

      inline void SetType(const TypeInfoRef& type)
      {
        featureValueBuffer.SetType(type);
      }

      inline void SetFeatures(const FeatureValueBuffer& buffer)
      {
        featureValueBuffer.Set(buffer);
      }
    };

  private:
    FileOffset        fileOffset;

  public:
    std::vector<Ring> rings;

  private:
    bool ReadIds(FileScanner& scanner,
                 uint32_t nodesCount,
                 std::vector<Id>& ids);

    bool WriteIds(FileWriter& writer,
                  const std::vector<Id>& ids) const;

  public:
    inline Area()
    : fileOffset(0)
    {
      // no code
    }

    inline FileOffset GetFileOffset() const
    {
      return fileOffset;
    }

    inline TypeId GetTypeId() const
    {
      return rings.front().GetTypeId();
    }

    inline TypeInfoRef GetType() const
    {
      return rings.front().GetType();
    }

    inline bool IsSimple() const
    {
      return rings.size()==1;
    }

    bool GetCenter(double& lat,
                   double& lon) const;
    void GetBoundingBox(double& minLon,
                        double& maxLon,
                        double& minLat,
                        double& maxLat) const;

    bool Read(const TypeConfig& typeConfig,
              FileScanner& scanner);
    bool ReadOptimized(const TypeConfig& typeConfig,
                       FileScanner& scanner);

    bool Write(const TypeConfig& typeConfig,
               FileWriter& writer) const;
    bool WriteOptimized(const TypeConfig& typeConfig,
                        FileWriter& writer) const;
  };

  typedef Ref<Area> AreaRef;
}

#endif
