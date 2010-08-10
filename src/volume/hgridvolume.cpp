#include <mitsuba/render/volume.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/plugin.h>

MTS_NAMESPACE_BEGIN

/**
 * This class implements a two-layer hierarchical grid
 * using 'gridvolume'-based files. It loads a dictionary
 * and then proceeds to map volume data into memory
 */
class HierarchicalGridDataSource : public VolumeDataSource {
public:
	HierarchicalGridDataSource(const Properties &props) 
		: VolumeDataSource(props) {
		m_volumeToWorld = props.getTransform("toWorld", Transform());
		m_prefix = props.getString("prefix");
		m_postfix = props.getString("postfix");
		std::string filename = props.getString("filename");
		loadDictionary(filename);
	}

	HierarchicalGridDataSource(Stream *stream, InstanceManager *manager) 
	: VolumeDataSource(stream, manager) {
		m_volumeToWorld = Transform(stream);
		std::string filename = stream->readString();
		m_prefix = stream->readString();
		m_postfix = stream->readString();
		loadDictionary(filename);
	}

	virtual ~HierarchicalGridDataSource() {
		size_t nCells = m_res.x*m_res.y*m_res.z;
		for (size_t i=0; i<nCells; ++i) {
			if (m_blocks[i] != NULL)
				m_blocks[i]->decRef();
		}
		delete[] m_blocks;
	}

	void serialize(Stream *stream, InstanceManager *manager) const {
		VolumeDataSource::serialize(stream, manager);

		m_volumeToWorld.serialize(stream);
		stream->writeString(m_filename);
		stream->writeString(m_prefix);
		stream->writeString(m_postfix);
	}

	void loadDictionary(const std::string &filename) {
		std::string resolved = FileResolver::getInstance()->resolve(filename);
		Log(EInfo, "Loading hierarchical grid dictionary \"%s\"", filename.c_str());
		ref<FileStream> stream = new FileStream(resolved, FileStream::EReadOnly);
		stream->setByteOrder(Stream::ELittleEndian);
		Float xmin = stream->readSingle(), ymin = stream->readSingle(), zmin = stream->readSingle();
		Float xmax = stream->readSingle(), ymax = stream->readSingle(), zmax = stream->readSingle();
		m_aabb = AABB(Point(xmin, ymin, zmin), Point(xmax, ymax, zmax));
		m_res = Vector3i(stream);
		m_filename = filename;
		size_t nCells = m_res.x*m_res.y*m_res.z;
		m_blocks = new VolumeDataSource*[nCells];
		memset(m_blocks, 0, nCells*sizeof(VolumeDataSource *));
		Vector extents = m_aabb.getExtents();
		m_worldToVolume = m_volumeToWorld.inverse();
		m_worldToGrid = Transform::scale(Vector(
				(m_res[0]) / extents[0],
				(m_res[1]) / extents[1],
				(m_res[2]) / extents[2])
			) * Transform::translate(-Vector(m_aabb.min)) * m_worldToVolume;

		m_supportsFloatLookups = true;
		m_supportsVectorLookups = true;
		m_supportsSpectrumLookups = true;
		m_stepSize = std::numeric_limits<Float>::infinity();

		int numBlocks = 0;
		while (!stream->isEOF()) {
			Vector3i block = Vector3i(stream);
			Assert(block.x >= 0 && block.y >= 0 && block.z >= 0 && block.x < m_res.x && block.y < m_res.y && block.z < m_res.z);
			Properties props("gridvolume");
			props.setString("filename", formatString("%s%03i_%03i_%03i%s", m_prefix.c_str(), block.x, block.y, block.z, m_postfix.c_str()));
			props.setTransform("toWorld", m_volumeToWorld);
			props.setBoolean("sendData", false);

			VolumeDataSource *content = static_cast<VolumeDataSource *> (PluginManager::getInstance()->
					createObject(VolumeDataSource::m_theClass, props));
			content->configure();

			m_blocks[(m_res.y * block.z + block.y) * m_res.x + block.x] = content;
			m_stepSize = std::min(m_stepSize, content->getStepSize());
			m_supportsVectorLookups = m_supportsVectorLookups && content->supportsVectorLookups();
			m_supportsFloatLookups = m_supportsFloatLookups && content->supportsFloatLookups();
			m_supportsSpectrumLookups = m_supportsSpectrumLookups && content->supportsSpectrumLookups();
			content->incRef();
			++numBlocks;
		}
		Log(EInfo, "%i blocks total, %s, stepSize=%f, resolution=%s", numBlocks, m_aabb.toString().c_str(), m_stepSize, m_res.toString().c_str());
	}

	bool supportsFloatLookups() const {
		return m_supportsFloatLookups;
	}

	bool supportsSpectrumLookups() const {
		return m_supportsSpectrumLookups;
	}

	bool supportsVectorLookups() const {
		return m_supportsVectorLookups;
	}

	Float getStepSize() const {
		return m_stepSize;
	}

	Float lookupFloat(const Point &_p) const {
		const Point p = m_worldToGrid.transformBasic(_p);
		int x = (int) p.x, y = (int) p.y, z = (int) p.z;
		if (x < 0 || x >= m_res.x ||
			y < 0 || y >= m_res.y ||
			z < 0 || z >= m_res.z)
			return 0.0f;

		VolumeDataSource *block = m_blocks[((z * m_res.y) + y) * m_res.x + x];
		if (block == NULL)
			return 0.0f;
		else
			return block->lookupFloat(_p);
	}

	Spectrum lookupSpectrum(const Point &_p) const {
		const Point p = m_worldToGrid.transformBasic(_p);
		int x = (int) p.x, y = (int) p.y, z = (int) p.z;
		if (x < 0 || x >= m_res.x ||
			y < 0 || y >= m_res.y ||
			z < 0 || z >= m_res.z)
			return Spectrum(0.0f);

		VolumeDataSource *block = m_blocks[((z * m_res.y) + y) * m_res.x + x];
		if (block == NULL)
			return Spectrum(0.0f);
		else
			return block->lookupSpectrum(_p);
	}

	Vector lookupVector(const Point &_p) const {
		const Point p = m_worldToGrid.transformBasic(_p);
		int x = (int) p.x, y = (int) p.y, z = (int) p.z;
		if (x < 0 || x >= m_res.x ||
			y < 0 || y >= m_res.y ||
			z < 0 || z >= m_res.z)
			return Vector();

		VolumeDataSource *block = m_blocks[((z * m_res.y) + y) * m_res.x + x];
		if (block == NULL)
			return Vector();
		else
			return block->lookupVector(_p);
	}


	MTS_DECLARE_CLASS()
protected:
	std::string m_filename, m_prefix, m_postfix;
	Transform m_volumeToWorld;
	Transform m_worldToVolume;
	Transform m_worldToGrid;
	VolumeDataSource **m_blocks;
	Vector3i m_res;
	size_t m_count;
	bool m_supportsFloatLookups;
	bool m_supportsSpectrumLookups;
	bool m_supportsVectorLookups;
	Float m_stepSize;
};

MTS_IMPLEMENT_CLASS_S(HierarchicalGridDataSource, false, VolumeDataSource);
MTS_EXPORT_PLUGIN(HierarchicalGridDataSource, "Hierarchical grid data source");
MTS_NAMESPACE_END