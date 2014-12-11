#include <cstring>
#include <iomanip>
#include <iostream>
#include <osgViewer/Viewer>

#include "cmp.h"

std::ostream& operator<<(std::ostream& lhs, cmp::Node::Type type)
{
	switch (type) {
		case cmp::Node::Root:      lhs << "Root";      break;
		case cmp::Node::Transform: lhs << "Transform"; break;
		case cmp::Node::Mesh1:	   lhs << "Mesh1";     break;
		case cmp::Node::Axis:	   lhs << "Axis";      break;
		case cmp::Node::Light:	   lhs << "Light";     break;
		case cmp::Node::Smoke:	   lhs << "Smoke";     break;
		case cmp::Node::Mesh2:	   lhs << "Mesh2";     break;
		default: lhs << "Unknown (" << (uint32_t)type << ")";
	}

	return lhs;
}

void printNode(cmp::Node* node)
{
	static int level = 0;

	int indent = 4 * level;
	
	std::cout << std::setw(indent) << "" << node->type << " node (\"" << node->name << "\")" << std::endl;

	switch (node->type) {
		case cmp::Node::Root:
		case cmp::Node::Transform:
		case cmp::Node::Axis:
		case cmp::Node::Light:
		case cmp::Node::Smoke:
			break;
		case cmp::Node::Mesh1:
		case cmp::Node::Mesh2:
			cmp::MeshNode* meshNode = dynamic_cast<cmp::MeshNode*>(node);
			if (meshNode) {
				for (cmp::Mesh* mesh : meshNode->meshes) {
					std::cout << std::setw(indent + 4) << "" << "Mesh \"" << mesh->name << "\" (" << mesh->length << ") bytes" << std::endl;
					std::cout << std::setw(indent + 8) << "" << mesh->vertexCount2 << " vertices" << std::endl;
					std::cout << std::setw(indent + 8) << "" << mesh->indexCount << " indices" << std::endl;
					std::cout << std::setw(indent + 8) << "" << mesh->unparsedLength << " unparsed bytes" << std::endl;
				}
			}
			break;
	}

	cmp::GroupNode* group = dynamic_cast<cmp::GroupNode*>(node);
	if (group) {
		level++;
		for (cmp::Node* node : group->children) {
			printNode(node);
		}
		level--;
	}
}

osg::ref_ptr<osg::Geode> drawBoundBox(cmp::BoundBox* aabb)
{
	osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array();

	vertices->push_back(osg::Vec3(aabb->max.x, aabb->max.y, aabb->max.z)); // 1 0
	vertices->push_back(osg::Vec3(aabb->min.x, aabb->max.y, aabb->max.z)); // 2 1
	vertices->push_back(osg::Vec3(aabb->max.x, aabb->min.y, aabb->max.z)); // 3 2
	vertices->push_back(osg::Vec3(aabb->min.x, aabb->min.y, aabb->max.z)); // 4 3

	vertices->push_back(osg::Vec3(aabb->max.x, aabb->max.y, aabb->min.z)); // 5 4
	vertices->push_back(osg::Vec3(aabb->min.x, aabb->max.y, aabb->min.z)); // 6 5
	vertices->push_back(osg::Vec3(aabb->max.x, aabb->min.y, aabb->min.z)); // 7 6
	vertices->push_back(osg::Vec3(aabb->min.x, aabb->min.y, aabb->min.z)); // 8 7

	osg::ref_ptr<osg::DrawElementsUInt> indices = new osg::DrawElementsUInt(osg::PrimitiveSet::LINES);

	indices->push_back(0);
	indices->push_back(1);
	indices->push_back(1);
	indices->push_back(3);
	indices->push_back(3);
	indices->push_back(2);
	indices->push_back(2);
	indices->push_back(0);

	indices->push_back(4);
	indices->push_back(5);
	indices->push_back(5);
	indices->push_back(7);
	indices->push_back(7);
	indices->push_back(6);
	indices->push_back(6);
	indices->push_back(4);

	indices->push_back(0);
	indices->push_back(4);
	indices->push_back(1);
	indices->push_back(5);
	indices->push_back(2);
	indices->push_back(6);
	indices->push_back(3);
	indices->push_back(7);

	osg::ref_ptr<osg::Geometry> geometry = new osg::Geometry();
	geometry->setVertexArray(vertices.get());
	geometry->addPrimitiveSet(indices.get());

	osg::ref_ptr<osg::Geode> geode = new osg::Geode();
	geode->addDrawable(geometry.get());

	return geode.get();
}

osg::ref_ptr<osg::Node> drawNode(cmp::Node* node)
{
	switch (node->type) {
		case cmp::Node::Root:
		case cmp::Node::Transform:
		{
			cmp::GroupNode* groupNode = dynamic_cast<cmp::GroupNode*>(node);

			osg::ref_ptr<osg::Group> group = new osg::Group();

			group->addChild(drawBoundBox(&groupNode->aabb));

			for (cmp::Node* node : groupNode->children) {
				 osg::ref_ptr<osg::Node> child = drawNode(node);
				 if (child) {
					group->addChild(child.get());
				}
			}

			return group.get();
		}
		case cmp::Node::Mesh1:
		case cmp::Node::Mesh2:
		{
			cmp::MeshNode* meshNode = dynamic_cast<cmp::MeshNode*>(node);

			osg::ref_ptr<osg::Group> group = new osg::Group();

			if (meshNode->hasBound()) {
				group->addChild(drawBoundBox(&meshNode->aabb));
			}

			for (cmp::Mesh* mesh : meshNode->meshes) {
				group->addChild(drawBoundBox(&mesh->aabb));
				break;
			}

			return group.get();
		}
		case cmp::Node::Axis:
		case cmp::Node::Light:
		case cmp::Node::Smoke:
			return 0;
	}
}

int main(int argc, char** argv)
{
	if (argc != 2) {
		std::cerr << "Usage: " << argv[0] << " filename.cmp" << std::endl;
		return 1;
	}

	std::ifstream ifs;
	ifs.exceptions(std::ifstream::failbit | std::ifstream::badbit | std::ifstream::eofbit);

	std::cout << "Reading \"" << argv[1] << "\"" << std::endl;

	cmp::RootNode* root;

	try {
		ifs.open(argv[1], std::ifstream::in | std::ifstream::binary | std::ifstream::ate);

		std::streampos length = ifs.tellg();
		ifs.seekg(0);

		root = cmp::RootNode::readFile(ifs);

		std::cout << "Finished reading with " << length - ifs.tellg() << " bytes left in file" << std::endl << std::endl;;

		ifs.close();

		printNode(root);
		std::cout << std::endl;
	}
	catch (const std::ios_base::failure& e) {
		std::cerr << "Exception: " << ::strerror(errno) << std::endl;
		return 2;
	}
	catch (const std::exception& e) {
		std::cerr << "Exception: " << e.what() << std::endl;
		return 3;
	}

	osg::ref_ptr<osg::Node> model = drawNode(root);

	delete root;

	osgViewer::Viewer viewer;
	viewer.setUpViewInWindow(0, 0, 800, 600);
	viewer.setSceneData(model.get());
	viewer.realize();

	return viewer.run();
}
