#include "MeshViewerCore.h"
#include "MeshCuboidEvaluator.h"
#include "MeshCuboidPredictor.h"
#include "MeshCuboidRelation.h"
#include "MeshCuboidTrainer.h"
#include "MeshCuboidSolver.h"
//#include "QGLOcculsionTestWidget.h"

#include <sstream>
#include <Eigen/Core>
#include <gflags/gflags.h>
#include <QDir>
#include <QFileInfo>


DEFINE_bool(run_training, false, "");
DEFINE_bool(run_prediction, false, "");

// To be removed.
//DEFINE_bool(use_symmetric_group_cuboids, false, "");

DEFINE_string(data_root_path, "D:/Data/shape2pose/", "");
DEFINE_string(label_info_path, "data/0_body/assembly_chairs/", "");
DEFINE_string(mesh_path, "data/1_input/assembly_chairs/off/", "");
DEFINE_string(sample_path, "data/2_analysis/assembly_chairs/points/even1000/", "");
DEFINE_string(dense_sample_path, "data/2_analysis/assembly_chairs/points/random100000/", "");
DEFINE_string(mesh_label_path, "data/1_input/assembly_chairs/gt/", "");
DEFINE_string(sample_label_path, "data/4_experiments/exp1_assembly_chairs/1_prediction/", "");
DEFINE_string(output_path, "output", "");

DEFINE_string(mesh_filename, "", "");

DEFINE_string(label_info_filename, "regions.txt", "");
DEFINE_string(label_symmetry_info_filename, "regions_symmetry.txt", "");
DEFINE_string(symmetry_group_info_filename, "symmetry_groups.txt", "");
DEFINE_string(pose_filename, "pose.txt", "");
//DEFINE_string(occlusion_pose_filename, "occlusion_pose.txt", "");
DEFINE_string(occlusion_pose_filename, "", "");

DEFINE_string(single_feature_filename_prefix, "single_feature_", "");
DEFINE_string(pair_feature_filename_prefix, "pair_feature_", "");
DEFINE_string(single_stats_filename_prefix, "single_stats_", "");
DEFINE_string(pair_stats_filename_prefix, "pair_stats_", "");
DEFINE_string(feature_filename_prefix, "feature_", "");
DEFINE_string(transformation_filename_prefix, "transformation_", "");
DEFINE_string(joint_normal_relation_filename_prefix, "joint_normal_", "");
DEFINE_string(cond_normal_relation_filename_prefix, "conditional_normal_", "");
DEFINE_string(object_list_filename, "object_list.txt", "");

DEFINE_double(occlusion_test_radius, 0.01, "");

const bool run_ground_truth = true;



void MeshViewerCore::parse_arguments()
{
	std::cout << "data_root_path = " << FLAGS_data_root_path << std::endl;
	std::cout << "label_info_path = " << FLAGS_data_root_path + FLAGS_label_info_path << std::endl;
	std::cout << "mesh_path = " << FLAGS_data_root_path + FLAGS_mesh_path << std::endl;
	std::cout << "sample_path = " << FLAGS_data_root_path + FLAGS_sample_path << std::endl;
	std::cout << "sample_label_path = " << FLAGS_data_root_path + FLAGS_sample_label_path << std::endl;
	std::cout << "mesh_label_path = " << FLAGS_data_root_path + FLAGS_mesh_label_path << std::endl;
	std::cout << "output_path = " << FLAGS_output_path << std::endl;

	if (FLAGS_run_training)
	{
		train();
		exit(EXIT_FAILURE);
	}
	else if (FLAGS_run_prediction)
	{
		std::cout << "mesh_filename = " << FLAGS_mesh_filename << std::endl;
		predict();
		exit(EXIT_FAILURE);
	}
}

void MeshViewerCore::open_cuboid_file(const char *_filename)
{
	assert(_filename);

	// Load basic information.
	bool ret = true;

	ret = ret & cuboid_structure_.load_labels((FLAGS_data_root_path +
		FLAGS_label_info_path + FLAGS_label_info_filename).c_str());
	ret = ret & cuboid_structure_.load_label_symmetries((FLAGS_data_root_path +
		FLAGS_label_info_path + FLAGS_label_symmetry_info_filename).c_str());

	if (!ret)
	{
		do {
			std::cout << "Error: Cannot open label information files.";
			std::cout << '\n' << "Press the Enter key to continue.";
		} while (std::cin.get() != '\n');
	}

	// To be removed.
	//if (FLAGS_use_symmetric_group_cuboids)
	//{
	//	cuboid_structure_.add_symmetric_group_labels();
	//}

	cuboid_structure_.load_cuboids(_filename);
	draw_cuboid_axes_ = true;
	setDrawMode(CUSTOM_VIEW);
	updateGL();
}

void MeshViewerCore::set_view_direction()
{
	Eigen::Vector3d view_direction;
	// -Z direction.
	view_direction << -modelview_matrix()[2], -modelview_matrix()[6], -modelview_matrix()[10];
	Eigen::Matrix3d rotation_mat;
	rotation_mat << modelview_matrix()[0], modelview_matrix()[4], modelview_matrix()[8],
		modelview_matrix()[1], modelview_matrix()[5], modelview_matrix()[9],
		modelview_matrix()[2], modelview_matrix()[6], modelview_matrix()[10];
	Eigen::Vector3d translation_vec;
	translation_vec << modelview_matrix()[12], modelview_matrix()[13], modelview_matrix()[14];
	Eigen::Vector3d view_point = -rotation_mat.transpose() * translation_vec;

	for (unsigned int i = 0; i < 3; ++i)
	{
		view_point_[i] = view_point[i];
		view_direction_[i] = view_direction[i];
	}

	update_cuboid_surface_points(cuboid_structure_, modelview_matrix());
	updateGL();
}

void MeshViewerCore::set_random_view_direction(bool _set_modelview_matrix)
{
	Eigen::Matrix4d centering_mat = Eigen::Matrix4d::Identity();
	for (int i = 0; i < 3; ++i)
		centering_mat(i, 3) = -mesh().get_bbox_center()[i];

	// Sample points on each face.
	srand(time(NULL));

	// latitude.
	double x_angle_min = -M_PI;
	double x_angle_max = 0;
	double x_angle = static_cast<double>(rand()) / RAND_MAX * (x_angle_max - x_angle_min) + x_angle_min;
	Eigen::Matrix4d x_axis_random_rotation_mat = Eigen::Matrix4d::Identity();
	x_axis_random_rotation_mat(1, 1) = cos(x_angle);
	x_axis_random_rotation_mat(1, 2) = -sin(x_angle);
	x_axis_random_rotation_mat(2, 1) = sin(x_angle);
	x_axis_random_rotation_mat(2, 2) = cos(x_angle);

	// longitude.
	double z_angle_min = 0;
	double z_angle_max = 2 * M_PI;
	double z_angle = static_cast<double>(rand()) / RAND_MAX * (z_angle_max - z_angle_min) + z_angle_min;
	Eigen::Matrix4d z_axis_random_rotation_mat = Eigen::Matrix4d::Identity();
	z_axis_random_rotation_mat(0, 0) = cos(z_angle);
	z_axis_random_rotation_mat(0, 1) = -sin(z_angle);
	z_axis_random_rotation_mat(1, 0) = sin(z_angle);
	z_axis_random_rotation_mat(1, 1) = cos(z_angle);


	const double x_offset = (static_cast<double>(rand()) / RAND_MAX - 0.5) * mesh().get_object_diameter();
	const double y_offset = (static_cast<double>(rand()) / RAND_MAX - 0.5) * mesh().get_object_diameter();
	const double z_offset = -1.5 * mesh().get_object_diameter();

	Eigen::Matrix4d translation_mat = Eigen::Matrix4d::Identity();
	translation_mat(0, 3) = x_offset;
	translation_mat(1, 3) = y_offset;
	translation_mat(2, 3) = z_offset;

	Eigen::Matrix4d transformation_mat = translation_mat
		* x_axis_random_rotation_mat * z_axis_random_rotation_mat * centering_mat;

	// -Z direction.
	Eigen::Vector3d view_direction = -transformation_mat.row(2).transpose().segment(0, 3);
	Eigen::Vector3d view_point = transformation_mat.col(3).segment(0, 3);

	for (unsigned int i = 0; i < 3; ++i)
	{
		view_point_[i] = view_point[i];
		view_direction_[i] = view_direction[i];
	}

	if (_set_modelview_matrix)
	{
		GLdouble matrix[16];
		for (int col = 0; col < 4; ++col)
			for (int row = 0; row < 4; ++row)
				matrix[4 * col + row] = transformation_mat(row, col);

		set_modelview_matrix(matrix);
	}

	update_cuboid_surface_points(cuboid_structure_, modelview_matrix());
	updateGL();
}

void MeshViewerCore::train()
{
	bool ret = true;
	ret = ret & cuboid_structure_.load_labels((FLAGS_data_root_path +
		FLAGS_label_info_path + FLAGS_label_info_filename).c_str());
	ret = ret & cuboid_structure_.load_label_symmetries((FLAGS_data_root_path +
		FLAGS_label_info_path + FLAGS_label_symmetry_info_filename).c_str());

	if (!ret)
	{
		do {
			std::cout << "Error: Cannot open label information files.";
			std::cout << '\n' << "Press the Enter key to continue.";
		} while (std::cin.get() != '\n');
	}

	// To be removed.
	//if (FLAGS_use_symmetric_group_cuboids)
	//{
	//	cuboid_structure_.add_symmetric_group_labels();
	//}

	unsigned int num_labels = cuboid_structure_.num_labels();


	std::ofstream mesh_name_list_file(FLAGS_object_list_filename);
	assert(mesh_name_list_file);

	std::vector< std::list<MeshCuboidFeatures *> > feature_list(num_labels);
	std::vector< std::list<MeshCuboidTransformation *> > transformation_list(num_labels);
	//std::vector< std::list<MeshCuboidAttributes *> > attributes_list(num_labels);
	//std::vector< std::list<MeshCuboidManualFeatures *> > manual_single_feature_list(num_labels);
	//std::vector< std::vector< std::list<MeshCuboidManualFeatures *> > > manual_pair_feature_list(num_labels);

	//for (LabelIndex label_index = 0; label_index < num_labels; ++label_index)
	//	manual_pair_feature_list[label_index].resize(num_labels);
	

	setDrawMode(CUSTOM_VIEW);
	open_modelview_matrix_file(FLAGS_pose_filename.c_str());

	// For every file in the base path.
	QDir input_dir((FLAGS_data_root_path + FLAGS_mesh_path).c_str());
	assert(input_dir.exists());
	input_dir.setFilter(QDir::Files | QDir::Hidden | QDir::NoSymLinks);
	input_dir.setSorting(QDir::Name);

	QDir output_dir;
	output_dir.mkpath(FLAGS_output_path.c_str());


	QFileInfoList dir_list = input_dir.entryInfoList();
	for (int i = 0; i < dir_list.size(); i++)
	{
		QFileInfo file_info = dir_list.at(i);
		if (file_info.exists() &&
			(file_info.suffix().compare("obj") == 0
			|| file_info.suffix().compare("off") == 0))
		{
			std::string mesh_name = std::string(file_info.baseName().toLocal8Bit());
			std::string mesh_filepath = std::string(file_info.filePath().toLocal8Bit());
			std::string mesh_label_filepath = FLAGS_data_root_path + FLAGS_mesh_label_path + std::string("/") + mesh_name + std::string(".seg");
			std::string sample_filepath = FLAGS_data_root_path + FLAGS_sample_path + std::string("/") + mesh_name + std::string(".pts");
			std::string sample_label_filepath = FLAGS_data_root_path + FLAGS_sample_label_path + std::string("/") + mesh_name + std::string(".arff");
			std::string snapshot_filepath = FLAGS_output_path + std::string("/") + mesh_name;

			QFileInfo mesh_file(mesh_filepath.c_str());
			QFileInfo sample_file(sample_filepath.c_str());
			QFileInfo sample_label_file(sample_label_filepath.c_str());
			QFileInfo mesh_label_file(mesh_label_filepath.c_str());

			if (!mesh_file.exists()
				|| !sample_file.exists()
				|| !sample_label_file.exists()
				|| !mesh_label_file.exists())
				continue;

			mesh_name_list_file << mesh_name << std::endl;



			cuboid_structure_.clear_cuboids();
			cuboid_structure_.clear_sample_points();

			open_mesh(mesh_filepath.c_str());
			open_mesh_face_label_file(mesh_label_filepath.c_str());
			open_sample_point_file(sample_filepath.c_str());

			if (run_ground_truth)
			{
				//cuboid_structure_.add_sample_points_from_mesh_vertices();
				cuboid_structure_.get_mesh_face_label_cuboids();
			}
			else
			{
				open_sample_point_label_file(sample_label_filepath.c_str());
				cuboid_structure_.compute_label_cuboids();
				cuboid_structure_.apply_mesh_face_labels_to_cuboids();
			}

			// Find the largest part for each part.
			cuboid_structure_.find_the_largest_label_cuboids();


			// To be removed.
			//if (FLAGS_use_symmetric_group_cuboids)
			//{
			//	cuboid_structure_.create_symmetric_group_cuboids();
			//}


			open_modelview_matrix_file(FLAGS_pose_filename.c_str());

			mesh_.clear_colors();
			updateGL();
			snapshot(snapshot_filepath.c_str());

			
			assert(cuboid_structure_.num_labels() == num_labels);
			for (LabelIndex label_index_1 = 0; label_index_1 < num_labels; ++label_index_1)
			{
				MeshCuboidTransformation *transformation = new MeshCuboidTransformation(mesh_name);
				//MeshCuboidAttributes *attributes = new MeshCuboidAttributes(mesh_name);
				MeshCuboidFeatures *features = new MeshCuboidFeatures(mesh_name);
				//MeshSingleCuboidManualFeatures *manual_single_features = new MeshSingleCuboidManualFeatures(mesh_name);

				Label label_1 = cuboid_structure_.labels_[label_index_1];
				MeshCuboid *cuboid_1 = NULL;
				
				// NOTE:
				// The current implementation assumes that there is only one part for each label.
				assert(cuboid_structure_.label_cuboids_[label_index_1].size() <= 1);
				if (!cuboid_structure_.label_cuboids_[label_index_1].empty())
					cuboid_1 = cuboid_structure_.label_cuboids_[label_index_1].front();

				//for (LabelIndex label_index_2 = label_index_1; label_index_2 < num_labels; ++label_index_2)
				//{
				//	MeshPairCuboidManualFeatures *manual_pair_features = new MeshPairCuboidManualFeatures(mesh_name);

				//	Label label_2 = cuboid_structure_.labels_[label_index_2];
				//	MeshCuboid *cuboid_2 = NULL;
				//	
				//	// NOTE:
				//	// The current implementation assumes that there is only one part for each label.
				//	assert(cuboid_structure_.label_cuboids_[label_index_2].size() <= 1);
				//	if (!cuboid_structure_.label_cuboids_[label_index_2].empty())
				//		cuboid_2 = cuboid_structure_.label_cuboids_[label_index_2].front();

				//	if (cuboid_1 && cuboid_2)
				//	{
				//		assert(label_index_1 <= label_index_2);
				//		manual_pair_features->compute_values(cuboid_1, cuboid_2);
				//		manual_pair_feature_list[label_index_1][label_index_2].push_back(manual_pair_features);
				//	}
				//}
				
				if (cuboid_1)
				{
					transformation->compute_transformation(cuboid_1);
					features->compute_features(cuboid_1);
					//manual_single_features->compute_values(cuboid_1);
				}

				transformation_list[label_index_1].push_back(transformation);
				feature_list[label_index_1].push_back(features);
				//manual_single_feature_list[label_index_1].push_back(manual_single_features);
			}
		}
	}


	for (LabelIndex label_index_1 = 0; label_index_1 < num_labels; ++label_index_1)
	{
		//for (LabelIndex label_index_2 = label_index_1; label_index_2 < num_labels; ++label_index_2)
		//{
		//	std::stringstream pair_features_filename_sstr;
		//	pair_features_filename_sstr << FLAGS_pair_feature_filename_prefix
		//		<< label_index_1 << std::string("_")
		//		<< label_index_2 << std::string(".csv");
		//	MeshCuboidManualFeatures::save_keys_and_values(
		//		manual_pair_feature_list[label_index_1][label_index_2],
		//		pair_features_filename_sstr.str().c_str());

		//	std::stringstream pair_stats_filename_sstr;
		//	pair_stats_filename_sstr << FLAGS_pair_stats_filename_prefix
		//		<< label_index_1 << std::string("_")
		//		<< label_index_2 << std::string(".csv");
		//	MeshCuboidManualFeatures::save_stats(
		//		manual_pair_feature_list[label_index_1][label_index_2],
		//		pair_stats_filename_sstr.str().c_str());
		//}

		std::stringstream transformation_filename_sstr;
		transformation_filename_sstr << FLAGS_transformation_filename_prefix
			<< label_index_1 << std::string(".csv");
		MeshCuboidTransformation::save_transformation_collection(transformation_filename_sstr.str().c_str(),
			transformation_list[label_index_1]);

		std::stringstream feature_filename_sstr;
		feature_filename_sstr << FLAGS_feature_filename_prefix
			<< label_index_1 << std::string(".csv");
		MeshCuboidFeatures::save_feature_collection(feature_filename_sstr.str().c_str(),
			feature_list[label_index_1]);

		//MeshCuboidAttributes::save_values(attributes_list[label_index_1],
		//	attributes_filename.c_str());

		//std::stringstream single_features_filename_sstr;
		//single_features_filename_sstr << FLAGS_single_feature_filename_prefix
		//	<< label_index_1 << std::string(".csv");
		//MeshCuboidManualFeatures::save_keys_and_values(manual_single_feature_list[label_index_1],
		//	single_features_filename_sstr.str().c_str());

		//std::stringstream single_stats_filename_sstr;
		//single_stats_filename_sstr << FLAGS_single_stats_filename_prefix
		//	<< label_index_1 << std::string(".csv");
		//MeshCuboidManualFeatures::save_stats(manual_single_feature_list[label_index_1],
		//	single_stats_filename_sstr.str().c_str());
	}


	// Deallocate.
	for (LabelIndex label_index_1 = 0; label_index_1 < num_labels; ++label_index_1)
	{
		//for (LabelIndex label_index_2 = label_index_1; label_index_2 < num_labels; ++label_index_2)
		//{
		//	for (std::list<MeshCuboidManualFeatures *>::iterator it = manual_pair_feature_list[label_index_1][label_index_2].begin();
		//		it != manual_pair_feature_list[label_index_1][label_index_2].end(); ++it)
		//		delete (*it);
		//}

		for (std::list<MeshCuboidTransformation *>::iterator it = transformation_list[label_index_1].begin();
			it != transformation_list[label_index_1].end(); ++it)
			delete (*it);

		//for (std::list<MeshCuboidAttributes *>::iterator it = attributes_list[label_index_1].begin();
		//	it != attributes_list[label_index_1].end(); ++it)
		//	delete (*it);

		for (std::list<MeshCuboidFeatures *>::iterator it = feature_list[label_index_1].begin();
			it != feature_list[label_index_1].end(); ++it)
			delete (*it);

		//for (std::list<MeshCuboidManualFeatures *>::iterator it = manual_single_feature_list[label_index_1].begin();
		//	it != manual_single_feature_list[label_index_1].end(); ++it)
		//	delete (*it);
	}

	mesh_name_list_file.close();

	std::cout << std::endl;
	std::cout << " -- Batch Completed. -- " << std::endl;
}

void MeshViewerCore::train_file_files()
{
	unsigned int num_cuboids = 0;

	MeshCuboidTrainer trainer;
	trainer.load_object_list(FLAGS_object_list_filename);
	trainer.load_features(FLAGS_feature_filename_prefix);
	trainer.load_transformations(FLAGS_transformation_filename_prefix);

	//
	std::vector< std::vector<MeshCuboidJointNormalRelations *> > joint_normal_relations;
	trainer.get_joint_normal_relations(joint_normal_relations);

	num_cuboids = joint_normal_relations.size();
	for (unsigned int cuboid_index_1 = 0; cuboid_index_1 < num_cuboids; ++cuboid_index_1)
	{
		assert(joint_normal_relations[cuboid_index_1].size() == num_cuboids);
		for (unsigned int cuboid_index_2 = 0; cuboid_index_2 < num_cuboids; ++cuboid_index_2)
		{
			if (cuboid_index_1 == cuboid_index_2) continue;

			const MeshCuboidJointNormalRelations *relation_12 = joint_normal_relations[cuboid_index_1][cuboid_index_2];
			if (!relation_12) continue;
			
			std::stringstream relation_filename_sstr;
			relation_filename_sstr << FLAGS_joint_normal_relation_filename_prefix << cuboid_index_1
				<< "_" << cuboid_index_2 << ".csv";

			std::cout << "Saving '" << relation_filename_sstr.str() << "'..." << std::endl;
			relation_12->save_joint_normal_csv(relation_filename_sstr.str().c_str());
		}
	}
	//

	////
	//std::vector< std::vector<MeshCuboidCondNormalRelations *> > cond_normal_relations;
	//trainer.get_cond_normal_relations(cond_normal_relations);

	//num_cuboids = cond_normal_relations.size();
	//for (unsigned int cuboid_index_1 = 0; cuboid_index_1 < num_cuboids; ++cuboid_index_1)
	//{
	//	assert(cond_normal_relations[cuboid_index_1].size() == num_cuboids);
	//	for (unsigned int cuboid_index_2 = 0; cuboid_index_2 < num_cuboids; ++cuboid_index_2)
	//	{
	//		if (cuboid_index_1 == cuboid_index_2) continue;

	//		const MeshCuboidCondNormalRelations *relation_12 = cond_normal_relations[cuboid_index_1][cuboid_index_2];
	//		if (!relation_12) continue;

	//		std::stringstream relation_filename_sstr;
	//		relation_filename_sstr << FLAGS_cond_normal_relation_filename_prefix << cuboid_index_1
	//			<< "_" << cuboid_index_2 << ".csv";

	//		std::cout << "Saving '" << relation_filename_sstr.str() << "'..." << std::endl;
	//		relation_12->save_cond_normal_csv(relation_filename_sstr.str().c_str());
	//	}
	//}
	////
}

void MeshViewerCore::batch_predict()
{
	// For every file in the base path.
	QDir input_dir((FLAGS_data_root_path + FLAGS_mesh_path).c_str());
	assert(input_dir.exists());
	input_dir.setFilter(QDir::Files | QDir::Hidden | QDir::NoSymLinks);
	input_dir.setSorting(QDir::Name);

	QFileInfoList dir_list = input_dir.entryInfoList();
	for (int file_index = 0; file_index < dir_list.size(); file_index++)
	{
		QFileInfo file_info = dir_list.at(file_index);
		if (file_info.exists() &&
			(file_info.suffix().compare("obj") == 0
			|| file_info.suffix().compare("off") == 0))
		{
			FLAGS_mesh_filename = std::string(file_info.fileName().toLocal8Bit());
			std::cout << "mesh_filename = " << FLAGS_mesh_filename << std::endl;
			predict();
		}
	}

	std::cout << " -- Batch Completed. -- " << std::endl;
}

void MeshViewerCore::predict()
{
	// Load basic information.
	bool ret = true;

	ret = ret & cuboid_structure_.load_labels((FLAGS_data_root_path +
		FLAGS_label_info_path + FLAGS_label_info_filename).c_str());
	ret = ret & cuboid_structure_.load_label_symmetries((FLAGS_data_root_path +
		FLAGS_label_info_path + FLAGS_label_symmetry_info_filename).c_str());

	// Load symmetry groups.
	ret = ret & cuboid_structure_.load_symmetry_groups((FLAGS_data_root_path +
		FLAGS_label_info_path + FLAGS_symmetry_group_info_filename).c_str());

	if (!ret)
	{
		do {
			std::cout << "Error: Cannot open label information files.";
			std::cout << '\n' << "Press the Enter key to continue.";
		} while (std::cin.get() != '\n');
	}


	// To be removed.
	//if (FLAGS_use_symmetric_group_cuboids)
	//{
	//	cuboid_structure_.add_symmetric_group_labels();
	//}


	ret = true;
	MeshCuboidTrainer trainer;
	ret = ret & trainer.load_object_list(FLAGS_object_list_filename);
	ret = ret & trainer.load_features(FLAGS_feature_filename_prefix);
	ret = ret & trainer.load_transformations(FLAGS_transformation_filename_prefix);

	if (!ret)
	{
		do {
			std::cout << "Error: Cannot open training files.";
			std::cout << '\n' << "Press the Enter key to continue.";
		} while (std::cin.get() != '\n');
	}


	// Check file paths.
	std::string mesh_filepath = FLAGS_data_root_path + FLAGS_mesh_path + std::string("/") + FLAGS_mesh_filename;
	QFileInfo mesh_file(mesh_filepath.c_str());
	std::string mesh_name = std::string(mesh_file.baseName().toLocal8Bit());

	std::string mesh_label_filepath = FLAGS_data_root_path + FLAGS_mesh_label_path + std::string("/") + mesh_name + std::string(".seg");
	std::string sample_filepath = FLAGS_data_root_path + FLAGS_sample_path + std::string("/") + mesh_name + std::string(".pts");
	std::string dense_sample_filepath = FLAGS_data_root_path + FLAGS_dense_sample_path + std::string("/") + mesh_name + std::string(".pts");
	std::string sample_label_filepath = FLAGS_data_root_path + FLAGS_sample_label_path + std::string("/") + mesh_name + std::string(".arff");

	QFileInfo mesh_label_file(mesh_label_filepath.c_str());
	QFileInfo sample_file(sample_filepath.c_str());
	QFileInfo sample_label_file(sample_label_filepath.c_str());

	if (!mesh_file.exists())
	{
		std::cerr << "Error: The mesh file does not exist (" << mesh_filepath << ")." << std::endl;
		return;
	}
	if (!mesh_label_file.exists())
	{
		std::cerr << "Warning: The mesh label file does not exist (" << mesh_label_filepath << ")." << std::endl;
	}
	if (!sample_file.exists())
	{
		std::cerr << "Error: The sample file does not exist (" << sample_filepath << ")." << std::endl;
		return;
	}
	if (!sample_label_file.exists())
	{
		std::cerr << "Error: The sample label file does not exist (" << sample_label_filepath << ")." << std::endl;
		return;
	}

	std::string filename_prefix = std::string("/") + mesh_name + std::string("_");
	std::stringstream snapshot_filename_sstr;
	std::stringstream log_filename_sstr;
	std::stringstream stats_filename_sstr;
	std::stringstream cuboid_filename_sstr;

	QDir output_dir;
	output_dir.mkpath(FLAGS_output_path.c_str());
	output_dir.mkpath((FLAGS_output_path + std::string("/Temp")).c_str());


	// Initialize basic information.
	unsigned int num_labels = cuboid_structure_.num_labels();
	cuboid_structure_.clear_cuboids();
	cuboid_structure_.clear_sample_points();

	std::list<std::string> ignored_object_list;
	ignored_object_list.push_back(mesh_name);

	std::vector< std::vector<MeshCuboidJointNormalRelations *> > joint_normal_relations;
	//MeshCuboidTrainer::load_joint_normal_relations(num_labels, "joint_normal_", joint_normal_relations);
	trainer.get_joint_normal_relations(joint_normal_relations, &ignored_object_list);
	MeshCuboidJointNormalRelationPredictor joint_normal_predictor(joint_normal_relations);

	//std::vector< std::vector<MeshCuboidCondNormalRelations *> > cond_normal_relations;
	//MeshCuboidTrainer::load_cond_normal_relations(num_labels, "conditional_normal_", cond_normal_relations);
	//trainer.get_cond_normal_relations(cond_normal_relations, &ignored_object_list);
	//MeshCuboidCondNormalRelationPredictor cond_normal_predictor(cond_normal_relations);


	// Load files.
	setDrawMode(CUSTOM_VIEW);

	open_mesh(mesh_filepath.c_str());

	if (mesh_label_file.exists())
	{
		open_mesh_face_label_file(mesh_label_filepath.c_str());
	}

	open_sample_point_file(sample_filepath.c_str());
	open_sample_point_label_file(sample_label_filepath.c_str());

	if (cuboid_structure_.num_sample_points() == 0)
	{
		std::cerr << "Error: No sample point loaded (" << sample_filepath << ")." << std::endl;
		return;
	}


	// Pre-processing.
	std::cout << "\n - Remove occluded points." << std::endl;
	if (FLAGS_occlusion_pose_filename == "")
	{
		set_random_view_direction(true);
		remove_occluded_points();
		//set_view_direction();
	}
	else
	{
		open_modelview_matrix_file(FLAGS_occlusion_pose_filename.c_str());
		remove_occluded_points();
		//do_occlusion_test();
		//set_view_direction();
	}

	double occlusion_modelview_matrix[16];
	memcpy(occlusion_modelview_matrix, modelview_matrix(), 16 * sizeof(double));

	//Eigen::VectorXd occlusion_test_values;
	//ANNpointArray occlusion_test_ann_points;
	//ANNkd_tree* occlusion_test_points_kd_tree = NULL;

	//occlusion_test_points_kd_tree = create_occlusion_test_points_kd_tree(
	//	occlusion_test_points_, cuboid_structure_,
	//	occlusion_test_values, occlusion_test_ann_points);
	//assert(occlusion_test_ann_points);
	//assert(occlusion_test_points_kd_tree);

	mesh_.clear_colors();
	updateGL();
	snapshot_filename_sstr.clear(); snapshot_filename_sstr.str("");
	snapshot_filename_sstr << FLAGS_output_path << filename_prefix << std::string("view");
	snapshot(snapshot_filename_sstr.str().c_str());

	open_modelview_matrix_file(FLAGS_pose_filename.c_str());
	updateGL();
	snapshot_filename_sstr.clear(); snapshot_filename_sstr.str("");
	snapshot_filename_sstr << FLAGS_output_path << filename_prefix << std::string("input");
	snapshot(snapshot_filename_sstr.str().c_str());


	std::cout << "\n - Cluster points and construct initial cuboids." << std::endl;
	draw_cuboid_axes_ = false;

	cuboid_structure_.compute_label_cuboids();
	// Remove cuboids in symmetric labels.
	cuboid_structure_.remove_symmetric_cuboids();

	if (cuboid_structure_.get_all_cuboids().empty())
		return;

	update_cuboid_surface_points(cuboid_structure_, occlusion_modelview_matrix);


	// Sub-routine.
	bool first_iteration = true;
	unsigned int num_final_cuboid_structure_candidates = 0;

	std::list< std::pair<std::string, MeshCuboidStructure> > cuboid_structure_candidates;
	cuboid_structure_candidates.push_back(std::make_pair(std::string("0"), cuboid_structure_));


	while (!cuboid_structure_candidates.empty())
	{
		// FIXME:
		// The cuboid structure should not deep copy all sample points.
		// Use smart pointers for sample points.
		std::string cuboid_structure_name = cuboid_structure_candidates.front().first;
		cuboid_structure_ = cuboid_structure_candidates.front().second;
		cuboid_structure_candidates.pop_front();

		unsigned int snapshot_index = 0;
		log_filename_sstr.clear(); log_filename_sstr.str("");
		log_filename_sstr << FLAGS_output_path + std::string("/Temp") << filename_prefix
			<< std::string("c_") << cuboid_structure_name << std::string("_log.txt");
		std::ofstream log_file(log_filename_sstr.str());
		log_file.clear(); log_file.close();


		updateGL();
		snapshot_filename_sstr.clear(); snapshot_filename_sstr.str("");
		snapshot_filename_sstr << FLAGS_output_path + std::string("/Temp") << filename_prefix
			<< std::string("c_") << cuboid_structure_name << std::string("_")
			<< std::string("s_") << snapshot_index;
		snapshot(snapshot_filename_sstr.str().c_str());
		++snapshot_index;
		draw_cuboid_axes_ = true;
		

		std::cout << "\n1. Recognize labels and axes configurations." << std::endl;
		// NOTE:
		// Use symmetric label information only at the first time of the iteration.
		recognize_labels_and_axes_configurations(cuboid_structure_,
			joint_normal_predictor, log_filename_sstr.str(), first_iteration,
			true);
		first_iteration = false;

		//
		cuboid_structure_.compute_symmetry_groups();
		//

		updateGL();
		snapshot_filename_sstr.clear(); snapshot_filename_sstr.str("");
		snapshot_filename_sstr << FLAGS_output_path + std::string("/Temp") << filename_prefix
			<< std::string("c_") << cuboid_structure_name << std::string("_")
			<< std::string("s_") << snapshot_index;
		snapshot(snapshot_filename_sstr.str().c_str());
		++snapshot_index;


		std::cout << "\n2. Segment sample points." << std::endl;
		segment_sample_points(cuboid_structure_);

		updateGL();
		snapshot_filename_sstr.clear(); snapshot_filename_sstr.str("");
		snapshot_filename_sstr << FLAGS_output_path + std::string("/Temp") << filename_prefix
			<< std::string("c_") << cuboid_structure_name << std::string("_")
			<< std::string("s_") << snapshot_index;
		snapshot(snapshot_filename_sstr.str().c_str());
		++snapshot_index;


		std::cout << "\n3. Optimize cuboid attributes." << std::endl;
		const double quadprog_ratio = 1E4;
		const unsigned int max_optimization_iteration = 5;

		optimize_attributes(cuboid_structure_, occlusion_modelview_matrix,
			joint_normal_predictor, quadprog_ratio, log_filename_sstr.str(),
			max_optimization_iteration, this);

		updateGL();
		snapshot_filename_sstr.clear(); snapshot_filename_sstr.str("");
		snapshot_filename_sstr << FLAGS_output_path + std::string("/Temp") << filename_prefix
			<< std::string("c_") << cuboid_structure_name << std::string("_")
			<< std::string("s_") << snapshot_index;
		snapshot(snapshot_filename_sstr.str().c_str());
		++snapshot_index;


		std::cout << "\n4. Add missing cuboids." << std::endl;
		assert(cuboid_structure_.num_labels() == num_labels);
		std::list<LabelIndex> given_label_indices;
		for (LabelIndex label_index = 0; label_index < num_labels; ++label_index)
			if (!cuboid_structure_.label_cuboids_[label_index].empty())
				given_label_indices.push_back(label_index);

		std::list< std::list<LabelIndex> > missing_label_index_groups;
		trainer.get_missing_label_index_groups(given_label_indices, missing_label_index_groups);
		
		if (!missing_label_index_groups.empty())
		{
			unsigned int missing_label_index_group_index = 0;

			for (std::list< std::list<LabelIndex> >::iterator it = missing_label_index_groups.begin();
				it != missing_label_index_groups.end(); ++it)
			{
				std::list<LabelIndex> &missing_label_indices = (*it);
				MeshCuboidStructure new_cuboid_structure = cuboid_structure_;

				add_missing_cuboids(new_cuboid_structure, occlusion_modelview_matrix,
					missing_label_indices, joint_normal_predictor);

				std::stringstream new_cuboid_structure_name;
				new_cuboid_structure_name << cuboid_structure_name << missing_label_index_group_index;
				cuboid_structure_candidates.push_front(
					std::make_pair(new_cuboid_structure_name.str(), new_cuboid_structure));
				++missing_label_index_group_index;
			}
		}
		else
		{
			// NOTE:
			// If there is no missing label, the next iteration becomes the last one.

			// Escape loop.
			updateGL();
			snapshot_filename_sstr.clear(); snapshot_filename_sstr.str("");
			snapshot_filename_sstr << FLAGS_output_path << filename_prefix
				<< num_final_cuboid_structure_candidates;
			snapshot(snapshot_filename_sstr.str().c_str());

			cuboid_filename_sstr.clear(); cuboid_filename_sstr.str("");
			cuboid_filename_sstr << FLAGS_output_path << filename_prefix
				<< num_final_cuboid_structure_candidates << std::string(".arff");
			cuboid_structure_.save_cuboids(cuboid_filename_sstr.str());

			/*
			if (mesh_label_file.exists())
			{
				stats_filename_sstr.clear(); stats_filename_sstr.str("");
				stats_filename_sstr << FLAGS_output_path << filename_prefix
					<< num_final_cuboid_structure_candidates << std::string("_stats.csv");

				MeshCuboidEvaluator evaluator(cuboid_structure_, mesh_name, cuboid_structure_name);
				evaluator.save_evaluate_results(stats_filename_sstr.str());
			}
			*/

			MeshCuboidStructure cuboid_structure_copy(cuboid_structure_);

			// Reconstruction using symmetry.
			cuboid_structure_.load_dense_sample_points(dense_sample_filepath.c_str());

			set_modelview_matrix(occlusion_modelview_matrix);
			remove_occluded_points();
			
			//
			updateGL();
			snapshot_filename_sstr.clear(); snapshot_filename_sstr.str("");
			snapshot_filename_sstr << FLAGS_output_path << filename_prefix
				<< num_final_cuboid_structure_candidates << std::string("_view_dense");
			snapshot(snapshot_filename_sstr.str().c_str());
			cuboid_structure_.save_sample_points_to_ply(snapshot_filename_sstr.str().c_str());
			//

			open_modelview_matrix_file(FLAGS_pose_filename.c_str());
			updateGL();
			cuboid_structure_.copy_sample_points_to_symmetric_position();
			cuboid_structure_.clear_cuboids();

			updateGL();
			snapshot_filename_sstr.clear(); snapshot_filename_sstr.str("");
			snapshot_filename_sstr << FLAGS_output_path << filename_prefix
				<< num_final_cuboid_structure_candidates << std::string("_reconstructed");
			snapshot(snapshot_filename_sstr.str().c_str());
			cuboid_structure_.save_sample_points_to_ply(snapshot_filename_sstr.str().c_str());
			cuboid_structure_ = cuboid_structure_copy;


			// Reconstruction using database.
			reconstruct_using_database();
			// NOTE:
			// Recover the original mesh after call 'reconstruct_using_database()'.
			open_mesh(mesh_filepath.c_str());
			open_modelview_matrix_file(FLAGS_pose_filename.c_str());
			updateGL();

			cuboid_structure_.clear_cuboids();

			updateGL();
			snapshot_filename_sstr.clear(); snapshot_filename_sstr.str("");
			snapshot_filename_sstr << FLAGS_output_path << filename_prefix
				<< num_final_cuboid_structure_candidates << std::string("_database");
			snapshot(snapshot_filename_sstr.str().c_str());
			cuboid_structure_.save_sample_points_to_ply(snapshot_filename_sstr.str().c_str());
			cuboid_structure_ = cuboid_structure_copy;


			// [TEST] Fusion.
			cuboid_structure_.load_dense_sample_points(dense_sample_filepath.c_str());

			set_modelview_matrix(occlusion_modelview_matrix);
			remove_occluded_points();
			open_modelview_matrix_file(FLAGS_pose_filename.c_str());
			updateGL();

			cuboid_structure_.copy_sample_points_to_symmetric_position();
			std::vector<LabelIndex> reconstructed_label_indices;
			reconstructed_label_indices.push_back(0);
			reconstructed_label_indices.push_back(1);
			reconstruct_using_database(&reconstructed_label_indices);
			// NOTE:
			// Recover the original mesh after call 'reconstruct_using_database()'.
			open_mesh(mesh_filepath.c_str());
			open_modelview_matrix_file(FLAGS_pose_filename.c_str());
			updateGL();

			cuboid_structure_.clear_cuboids();

			updateGL();
			snapshot_filename_sstr.clear(); snapshot_filename_sstr.str("");
			snapshot_filename_sstr << FLAGS_output_path << filename_prefix
				<< num_final_cuboid_structure_candidates << std::string("_fusion");
			snapshot(snapshot_filename_sstr.str().c_str());
			cuboid_structure_.save_sample_points_to_ply(snapshot_filename_sstr.str().c_str());
			cuboid_structure_ = cuboid_structure_copy;


			++num_final_cuboid_structure_candidates;
		}
	}


	//annDeallocPts(occlusion_test_ann_points);
	//delete occlusion_test_points_kd_tree;

	for (LabelIndex label_index_1 = 0; label_index_1 < joint_normal_relations.size(); ++label_index_1)
		for (LabelIndex label_index_2 = 0; label_index_2 < joint_normal_relations[label_index_1].size(); ++label_index_2)
			delete joint_normal_relations[label_index_1][label_index_2];

	//for (LabelIndex label_index_1 = 0; label_index_1 < cond_normal_relations.size(); ++label_index_1)
	//	for (LabelIndex label_index_2 = 0; label_index_2 < cond_normal_relations[label_index_1].size(); ++label_index_2)
	//		delete cond_normal_relations[label_index_1][label_index_2];
}

void MeshViewerCore::batch_render_point_clusters()
{
	bool ret = true;
	ret = ret & cuboid_structure_.load_labels((FLAGS_data_root_path +
		FLAGS_label_info_path + FLAGS_label_info_filename).c_str());
	ret = ret & cuboid_structure_.load_label_symmetries((FLAGS_data_root_path +
		FLAGS_label_info_path + FLAGS_label_symmetry_info_filename).c_str());
	if (!ret)
	{
		do {
			std::cout << "Error: Cannot open label information files.";
			std::cout << '\n' << "Press the Enter key to continue.";
		} while (std::cin.get() != '\n');
	}


	// To be removed.
	//if (FLAGS_use_symmetric_group_cuboids)
	//{
	//	cuboid_structure_.add_symmetric_group_labels();
	//}


	unsigned int num_labels = cuboid_structure_.num_labels();


	setDrawMode(CUSTOM_VIEW);
	open_modelview_matrix_file(FLAGS_pose_filename.c_str());

	// For every file in the base path.
	QDir input_dir((FLAGS_data_root_path + FLAGS_mesh_path).c_str());
	assert(input_dir.exists());
	input_dir.setFilter(QDir::Files | QDir::Hidden | QDir::NoSymLinks);
	input_dir.setSorting(QDir::Name);

	QDir output_dir;
	output_dir.mkpath(FLAGS_output_path.c_str());


	QFileInfoList dir_list = input_dir.entryInfoList();
	for (int i = 0; i < dir_list.size(); i++)
	{
		QFileInfo file_info = dir_list.at(i);
		if (file_info.exists() &&
			(file_info.suffix().compare("obj") == 0
			|| file_info.suffix().compare("off") == 0))
		{
			std::string mesh_name = std::string(file_info.baseName().toLocal8Bit());
			std::string mesh_filename = std::string(file_info.filePath().toLocal8Bit());

			std::string sample_filename = FLAGS_data_root_path + FLAGS_sample_path + std::string("/") + mesh_name + std::string(".pts");
			std::string sample_label_filename = FLAGS_data_root_path + FLAGS_sample_label_path + std::string("/") + mesh_name + std::string(".arff");
			std::string mesh_label_filename = FLAGS_data_root_path + FLAGS_mesh_label_path + std::string("/") + mesh_name + std::string(".seg");
			std::string snapshot_filename = FLAGS_output_path + std::string("/") + mesh_name + std::string("_in");

			QFileInfo mesh_file(mesh_filename.c_str());
			QFileInfo sample_file(sample_filename.c_str());
			QFileInfo sample_label_file(sample_label_filename.c_str());
			QFileInfo mesh_label_file(mesh_label_filename.c_str());

			if (!mesh_file.exists()
				|| !sample_file.exists()
				|| !sample_label_file.exists()
				|| !mesh_label_file.exists())
				continue;


			cuboid_structure_.clear_cuboids();
			cuboid_structure_.clear_sample_points();

			open_mesh(mesh_filename.c_str());
			open_sample_point_file(sample_filename.c_str());
			open_sample_point_label_file(sample_label_filename.c_str());

			cuboid_structure_.compute_label_cuboids();

			mesh_.clear_colors();
			draw_cuboid_axes_ = false;
			updateGL();
			snapshot(snapshot_filename.c_str());
		}
	}

	draw_cuboid_axes_ = true;
}

void MeshViewerCore::batch_render_cuboids()
{
	bool ret = true;
	ret = ret & cuboid_structure_.load_labels((FLAGS_data_root_path +
		FLAGS_label_info_path + FLAGS_label_info_filename).c_str());
	ret = ret & cuboid_structure_.load_label_symmetries((FLAGS_data_root_path +
		FLAGS_label_info_path + FLAGS_label_symmetry_info_filename).c_str());
	if (!ret)
	{
		do {
			std::cout << "Error: Cannot open label information files.";
			std::cout << '\n' << "Press the Enter key to continue.";
		} while (std::cin.get() != '\n');
	}


	// To be removed.
	//if (FLAGS_use_symmetric_group_cuboids)
	//{
	//	cuboid_structure_.add_symmetric_group_labels();
	//}


	unsigned int num_labels = cuboid_structure_.num_labels();


	setDrawMode(CUSTOM_VIEW);
	open_modelview_matrix_file(FLAGS_pose_filename.c_str());

	// For every file in the base path.
	QDir input_dir(FLAGS_output_path.c_str());
	assert(input_dir.exists());
	input_dir.setFilter(QDir::Files | QDir::Hidden | QDir::NoSymLinks);
	input_dir.setSorting(QDir::Name);

	QDir output_dir;
	output_dir.mkpath((FLAGS_output_path + std::string("/cuboid_snapshots/")).c_str());


	QFileInfoList dir_list = input_dir.entryInfoList();
	for (int i = 0; i < dir_list.size(); i++)
	{
		QFileInfo file_info = dir_list.at(i);
		if (file_info.exists() &&
			file_info.suffix().compare("arff") == 0)
		{
			std::string cuboid_name = std::string(file_info.baseName().toLocal8Bit());
			std::string cuboid_filename = std::string(file_info.filePath().toLocal8Bit());
			std::string snapshot_filename = FLAGS_output_path + std::string("/cuboid_snapshots/") + cuboid_name;

			QFileInfo cuboid_file(cuboid_filename.c_str());
			if (!cuboid_file.exists())
				continue;

			cuboid_structure_.clear_cuboids();
			cuboid_structure_.clear_sample_points();
			cuboid_structure_.load_cuboids(cuboid_filename.c_str());

			draw_cuboid_axes_ = true;
			updateGL();
			snapshot(snapshot_filename.c_str());
		}
	}
}

void MeshViewerCore::batch_render_points()
{
	bool ret = true;
	ret = ret & cuboid_structure_.load_labels((FLAGS_data_root_path +
		FLAGS_label_info_path + FLAGS_label_info_filename).c_str());
	ret = ret & cuboid_structure_.load_label_symmetries((FLAGS_data_root_path +
		FLAGS_label_info_path + FLAGS_label_symmetry_info_filename).c_str());
	if (!ret)
	{
		do {
			std::cout << "Error: Cannot open label information files.";
			std::cout << '\n' << "Press the Enter key to continue.";
		} while (std::cin.get() != '\n');
	}


	// To be removed.
	//if (FLAGS_use_symmetric_group_cuboids)
	//{
	//	cuboid_structure_.add_symmetric_group_labels();
	//}


	unsigned int num_labels = cuboid_structure_.num_labels();


	setDrawMode(CUSTOM_VIEW);
	open_modelview_matrix_file(FLAGS_pose_filename.c_str());

	// For every file in the base path.
	QDir input_dir((FLAGS_data_root_path + FLAGS_mesh_path).c_str());
	assert(input_dir.exists());
	input_dir.setFilter(QDir::Files | QDir::Hidden | QDir::NoSymLinks);
	input_dir.setSorting(QDir::Name);

	QDir output_dir;
	output_dir.mkpath((FLAGS_output_path + std::string("/reconstruction_snapshots/")).c_str());


	const std::string sample_filename_postfix("_0_reconstructed.pts");

	QFileInfoList dir_list = input_dir.entryInfoList();
	for (int i = 0; i < dir_list.size(); i++)
	{
		QFileInfo file_info = dir_list.at(i);
		if (file_info.exists() &&
			(file_info.suffix().compare("obj") == 0
			|| file_info.suffix().compare("off") == 0))
		{
			std::string mesh_name = std::string(file_info.baseName().toLocal8Bit());
			std::string mesh_filename = std::string(file_info.filePath().toLocal8Bit());

			//std::string sample_filename = FLAGS_data_root_path + FLAGS_sample_path + std::string("/") + mesh_name + std::string(".pts");
			std::string sample_filename = FLAGS_output_path + std::string("/") + mesh_name + sample_filename_postfix;
			std::string snapshot_filename = FLAGS_output_path + std::string("/reconstruction_snapshots/") + mesh_name;

			QFileInfo mesh_file(mesh_filename.c_str());
			QFileInfo sample_file(sample_filename.c_str());

			if (!mesh_file.exists()
				|| !sample_file.exists())
				continue;


			cuboid_structure_.clear_cuboids();
			cuboid_structure_.clear_sample_points();

			open_mesh(mesh_filename.c_str());
			open_sample_point_file(sample_filename.c_str());

			mesh_.clear_colors();
			draw_cuboid_axes_ = false;
			updateGL();
			snapshot(snapshot_filename.c_str());
		}
	}

	draw_cuboid_axes_ = true;
}

void MeshViewerCore::run_test()
{
	const char *_filename = "C:/project/app/cuboid-prediction/experiments/exp1_assembly_chairs/output/4138_0.arff";
	assert(_filename);

	// Load basic information.
	bool ret = true;

	ret = ret & cuboid_structure_.load_labels((FLAGS_data_root_path +
		FLAGS_label_info_path + FLAGS_label_info_filename).c_str());
	ret = ret & cuboid_structure_.load_label_symmetries((FLAGS_data_root_path +
		FLAGS_label_info_path + FLAGS_label_symmetry_info_filename).c_str());

	if (!ret)
	{
		do {
			std::cout << "Error: Cannot open label information files.";
			std::cout << '\n' << "Press the Enter key to continue.";
		} while (std::cin.get() != '\n');
	}

	cuboid_structure_.load_cuboids(_filename);
	draw_cuboid_axes_ = true;
	setDrawMode(CUSTOM_VIEW);
	updateGL();

	std::vector<LabelIndex> reconstructed_label_indices;
	reconstructed_label_indices.push_back(0);
	reconstructed_label_indices.push_back(1);
	reconstruct_using_database(&reconstructed_label_indices);
}

void MeshViewerCore::reconstruct_using_database(const std::vector<LabelIndex> *_reconstructed_label_indices)
{
	MeshCuboidStructure given_cuboid_structure(cuboid_structure_);
	cuboid_structure_.clear();

	bool ret = true;
	ret = ret & cuboid_structure_.load_labels((FLAGS_data_root_path +
		FLAGS_label_info_path + FLAGS_label_info_filename).c_str());
	ret = ret & cuboid_structure_.load_label_symmetries((FLAGS_data_root_path +
		FLAGS_label_info_path + FLAGS_label_symmetry_info_filename).c_str());

	// Load symmetry groups.
	ret = ret & cuboid_structure_.load_symmetry_groups((FLAGS_data_root_path +
		FLAGS_label_info_path + FLAGS_symmetry_group_info_filename).c_str());

	if (!ret)
	{
		do {
			std::cout << "Error: Cannot open label information files.";
			std::cout << '\n' << "Press the Enter key to continue.";
		} while (std::cin.get() != '\n');
	}

	unsigned int num_labels = given_cuboid_structure.num_labels();


	setDrawMode(CUSTOM_VIEW);
	open_modelview_matrix_file(FLAGS_pose_filename.c_str());

	// For every file in the base path.
	QDir input_dir((FLAGS_data_root_path + FLAGS_mesh_path).c_str());
	assert(input_dir.exists());
	input_dir.setFilter(QDir::Files | QDir::Hidden | QDir::NoSymLinks);
	input_dir.setSorting(QDir::Name);

	QDir output_dir;
	output_dir.mkpath(FLAGS_output_path.c_str());


	// (Dissimilarity, File information)
	std::vector< std::pair<Real, QFileInfo> > label_matched_object(num_labels);
	for (unsigned int label_index = 0; label_index < num_labels; ++label_index)
		label_matched_object[label_index].first = std::numeric_limits<Real>::max();


	QFileInfoList dir_list = input_dir.entryInfoList();
	for (int i = 0; i < dir_list.size(); i++)
	{
		QFileInfo file_info = dir_list.at(i);
		if (file_info.exists() &&
			(file_info.suffix().compare("obj") == 0
			|| file_info.suffix().compare("off") == 0))
		{
			std::string mesh_name = std::string(file_info.baseName().toLocal8Bit());
			std::string mesh_filepath = std::string(file_info.filePath().toLocal8Bit());
			std::string mesh_label_filepath = FLAGS_data_root_path + FLAGS_mesh_label_path + std::string("/") + mesh_name + std::string(".seg");
			std::string sample_filepath = FLAGS_data_root_path + FLAGS_sample_path + std::string("/") + mesh_name + std::string(".pts");
			std::string dense_sample_filepath = FLAGS_data_root_path + FLAGS_dense_sample_path + std::string("/") + mesh_name + std::string(".pts");
			std::string sample_label_filepath = FLAGS_data_root_path + FLAGS_sample_label_path + std::string("/") + mesh_name + std::string(".arff");
			std::string snapshot_filepath = FLAGS_output_path + std::string("/") + mesh_name;

			QFileInfo mesh_file(mesh_filepath.c_str());
			QFileInfo sample_file(sample_filepath.c_str());
			QFileInfo sample_label_file(sample_label_filepath.c_str());
			QFileInfo mesh_label_file(mesh_label_filepath.c_str());

			if (!mesh_file.exists()
				|| !sample_file.exists()
				|| !sample_label_file.exists()
				|| !mesh_label_file.exists())
				continue;

			cuboid_structure_.clear_cuboids();
			cuboid_structure_.clear_sample_points();

			open_mesh(mesh_filepath.c_str());
			open_mesh_face_label_file(mesh_label_filepath.c_str());
			open_sample_point_file(sample_filepath.c_str());
			cuboid_structure_.get_mesh_face_label_cuboids();

			// Find the largest part for each part.
			cuboid_structure_.find_the_largest_label_cuboids();

			//open_modelview_matrix_file(FLAGS_pose_filename.c_str());
			//mesh_.clear_colors();
			//updateGL();

			assert(cuboid_structure_.num_labels() == num_labels);
			for (LabelIndex label_index = 0; label_index < num_labels; ++label_index)
			{
				assert(cuboid_structure_.label_cuboids_[label_index].size() <= 1);
				MeshCuboid *cuboid = NULL;
				if (!cuboid_structure_.label_cuboids_[label_index].empty())
					cuboid = cuboid_structure_.label_cuboids_[label_index].front();

				assert(given_cuboid_structure.label_cuboids_[label_index].size() <= 1);
				MeshCuboid *given_cuboid = NULL;
				if (!given_cuboid_structure.label_cuboids_[label_index].empty())
					given_cuboid = given_cuboid_structure.label_cuboids_[label_index].front();

				if (cuboid && given_cuboid)
				{
					// Measure similarity.
					MyMesh::Normal cuboid_size = cuboid->get_bbox_size();
					MyMesh::Normal given_cuboid_size = given_cuboid->get_bbox_size();
					MyMesh::Normal diff_size = (cuboid_size - given_cuboid_size);

					Real dissimilarity = 0.0;
					for (unsigned int i = 0; i < 3; ++i)
						dissimilarity += std::abs(diff_size[i]);

					if (dissimilarity < label_matched_object[label_index].first)
					{
						label_matched_object[label_index].first = dissimilarity;
						label_matched_object[label_index].second = file_info;
					}
				}
			}
		}
	}


	// ---- //
	if (_reconstructed_label_indices)
	{
		given_cuboid_structure.clear_label_sample_points(*_reconstructed_label_indices);
	}
	else
	{
		given_cuboid_structure.clear_sample_points();
	}

	for (LabelIndex label_index = 0; label_index < num_labels; ++label_index)
	{
		//
		// Reconstruct designated labels only if '_reconstructed_label_indices' is provided.
		if (_reconstructed_label_indices)
		{
			bool exist = false;
			for (std::vector<LabelIndex>::const_iterator it = (*_reconstructed_label_indices).begin();
				it != (*_reconstructed_label_indices).end(); ++it)
			{
				if ((*it) == label_index)
				{
					exist = true;
					break;
				}
			}

			if (!exist) continue;
		}
		//

		if (label_matched_object[label_index].first == std::numeric_limits<Real>::max())
			continue;

		QFileInfo file_info = label_matched_object[label_index].second;
		std::string mesh_name = std::string(file_info.baseName().toLocal8Bit());
		std::string mesh_filepath = std::string(file_info.filePath().toLocal8Bit());
		std::string mesh_label_filepath = FLAGS_data_root_path + FLAGS_mesh_label_path + std::string("/") + mesh_name + std::string(".seg");
		std::string sample_filepath = FLAGS_data_root_path + FLAGS_sample_path + std::string("/") + mesh_name + std::string(".pts");
		std::string dense_sample_filepath = FLAGS_data_root_path + FLAGS_dense_sample_path + std::string("/") + mesh_name + std::string(".pts");
		std::string sample_label_filepath = FLAGS_data_root_path + FLAGS_sample_label_path + std::string("/") + mesh_name + std::string(".arff");
		std::string snapshot_filepath = FLAGS_output_path + std::string("/") + mesh_name;

		QFileInfo mesh_file(mesh_filepath.c_str());
		QFileInfo sample_file(sample_filepath.c_str());
		QFileInfo sample_label_file(sample_label_filepath.c_str());
		QFileInfo mesh_label_file(mesh_label_filepath.c_str());

		std::cout << "[" << label_index << "]: " << mesh_name << std::endl;

		if (!mesh_file.exists()
			|| !sample_file.exists()
			|| !sample_label_file.exists()
			|| !mesh_label_file.exists())
			continue;

		cuboid_structure_.clear_cuboids();
		cuboid_structure_.clear_sample_points();

		open_mesh(mesh_filepath.c_str());
		open_mesh_face_label_file(mesh_label_filepath.c_str());
		open_sample_point_file(sample_filepath.c_str());
		cuboid_structure_.get_mesh_face_label_cuboids();

		// Find the largest part for each part.
		cuboid_structure_.find_the_largest_label_cuboids();

		//
		cuboid_structure_.load_dense_sample_points(dense_sample_filepath.c_str());
		//

		assert(cuboid_structure_.label_cuboids_[label_index].size() <= 1);
		MeshCuboid *cuboid = NULL;
		if (!cuboid_structure_.label_cuboids_[label_index].empty())
			cuboid = cuboid_structure_.label_cuboids_[label_index].front();

		assert(given_cuboid_structure.label_cuboids_[label_index].size() <= 1);
		MeshCuboid *given_cuboid = NULL;
		if (!given_cuboid_structure.label_cuboids_[label_index].empty())
			given_cuboid = given_cuboid_structure.label_cuboids_[label_index].front();

		assert(cuboid);
		assert(given_cuboid);

		const int num_points = cuboid->num_sample_points();
		std::cout << "[" << label_index << "]: " << num_points << std::endl;
		for (int point_index = 0; point_index < num_points; ++point_index)
		{
			const MeshSamplePoint* sample_point = cuboid->get_sample_point(point_index);

			MyMesh::Point point = sample_point->point_;
			MyMesh::Normal normal = sample_point->normal_;

			//
			MyMesh::Point relative_point;
			MyMesh::Normal relative_normal;
			point = point - cuboid->get_bbox_center();

			for (unsigned int axis_index = 0; axis_index < 3; ++axis_index)
			{
				assert(cuboid->get_bbox_size()[axis_index] > 0);
				relative_point[axis_index] = dot(point, cuboid->get_bbox_axis(axis_index));
				relative_point[axis_index] *= 
					(given_cuboid->get_bbox_size()[axis_index] / cuboid->get_bbox_size()[axis_index]);

				relative_normal[axis_index] = dot(normal, cuboid->get_bbox_axis(axis_index));
				relative_normal[axis_index] *=
					(given_cuboid->get_bbox_size()[axis_index] / cuboid->get_bbox_size()[axis_index]);
			}

			point = given_cuboid->get_bbox_center();
			normal = MyMesh::Normal(0.0);

			for (unsigned int axis_index = 0; axis_index < 3; ++axis_index)
			{
				point += (relative_point[axis_index] * given_cuboid->get_bbox_axis(axis_index));
				normal += (relative_normal[axis_index] * given_cuboid->get_bbox_axis(axis_index));
			}

			normal.normalize();
			//

			MeshSamplePoint *new_sample_point = given_cuboid_structure.add_sample_point(point, normal);

			// Copy label confidence values.
			new_sample_point->label_index_confidence_ = sample_point->label_index_confidence_;

			given_cuboid->add_sample_point(new_sample_point);
		}
	}
	// ---- //

	cuboid_structure_ = given_cuboid_structure;
}

/*
void MeshViewerCore::do_occlusion_test()
{
	assert(occlusion_test_widget_);

	std::vector<Eigen::Vector3f> sample_points;
	sample_points.reserve(cuboid_structure_.num_sample_points());
	for (unsigned point_index = 0; point_index < cuboid_structure_.num_sample_points();
		++point_index)
	{
		MyMesh::Point point = cuboid_structure_.sample_points_[point_index]->point_;
		Eigen::Vector3f point_vec;
		point_vec << point[0], point[1], point[2];
		sample_points.push_back(point_vec);
	}

	occlusion_test_widget_->set_sample_points(sample_points);
	occlusion_test_widget_->set_modelview_matrix(modelview_matrix_);

	std::array<Eigen::Matrix<float, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>, 4> occlusion_test_result;
	occlusion_test_widget_->get_occlusion_test_result(
		static_cast<float>(FLAGS_occlusion_test_radius), occlusion_test_result);

	occlusion_test_points_.clear();
	occlusion_test_points_.reserve(occlusion_test_result[0].rows() * occlusion_test_result[0].cols());

	for (unsigned int row = 0; row < occlusion_test_result[0].rows(); ++row)
	{
		for (unsigned int col = 0; col < occlusion_test_result[0].cols(); ++col)
		{
			Mesh::Point point;
			for (unsigned int i = 0; i < 3; ++i)
				point[i] = static_cast<Real>(occlusion_test_result[i](row, col));
			Real value = static_cast<Real>(occlusion_test_result[3](row, col));
			assert(value >= 0);
			assert(value <= 1);

			// NOTE:
			// Reverse values.
			// After reversing, the value '1' means that the voxel is perfectly occluded.
			occlusion_test_points_.push_back(std::make_pair(point, 1.0 - value));
		}
	}

	draw_occlusion_test_points_ = true;
	//updateGL();
}

ANNkd_tree* create_occlusion_test_points_kd_tree(
	const std::vector< std::pair<MyMesh::Point, Real> > &_occlusion_test_points,
	const MeshCuboidStructure &_cuboid_structure,
	Eigen::VectorXd &_values,
	ANNpointArray &_ann_points)
{
	Eigen::MatrixXd points = Eigen::MatrixXd(3, _occlusion_test_points.size());
	_values = Eigen::VectorXd(_occlusion_test_points.size());

	for (unsigned point_index = 0; point_index < _occlusion_test_points.size();
		++point_index)
	{
		MyMesh::Point point = _occlusion_test_points[point_index].first;
		Eigen::Vector3d point_vec;
		point_vec << point[0], point[1], point[2];
		points.col(point_index) = point_vec;
		_values(point_index) = _occlusion_test_points[point_index].second;
	}
	
	ANNkd_tree* kd_tree = ICP::create_kd_tree(points, _ann_points);
	return kd_tree;
}
*/
