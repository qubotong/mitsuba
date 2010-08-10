#if !defined(__VOLUME_H)
#define __VOLUME_H

#include <mitsuba/core/fstream.h>
#include <mitsuba/core/aabb.h>

MTS_NAMESPACE_BEGIN

/**
 * Generalized source of volumetric information
 */
class MTS_EXPORT_RENDER VolumeDataSource : public ConfigurableObject {
public:
	/// Serialize to a binary data stream.
	virtual void serialize(Stream *stream, InstanceManager *manager) const;

	/// Return the bounding box
	inline const AABB &getAABB() const {
		return m_aabb;
	}

	/// Are float-valued lookups permitted?
	virtual bool supportsFloatLookups() const;

	/// Look up a floating point value by position
	virtual Float lookupFloat(const Point &p) const;

	/// Are spectrum-valued lookups permitted?
	virtual bool supportsSpectrumLookups() const;

	/// Look up a spectrum value by position
	virtual Spectrum lookupSpectrum(const Point &p) const;

	/// Are vector-valued lookups permitted?
	virtual bool supportsVectorLookups() const;

	/// Look up a vector value by position
	virtual Vector lookupVector(const Point &p) const;

	/**
	 * Returns the recommended step size for numerical 
	 * integration or inifinity if this is not known/applicable
	 */
	virtual Float getStepSize() const = 0;

	MTS_DECLARE_CLASS()
protected:
	/// Virtual destructor
	virtual ~VolumeDataSource();

	/// Protected constructor
	VolumeDataSource(const Properties &props);

	/// Unserialize from a binary data stream
	VolumeDataSource(Stream *stream, InstanceManager *manager); 
protected:
	AABB m_aabb;
};

MTS_NAMESPACE_END

#endif /* __VOLUME_H */