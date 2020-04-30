#include "reader_annotations.hh"
#include "method.hh"

void reader_annotations::read_annotations_file(std::string filename, std::string symbol_name, std::map<std::string, annotation *> &data) {
	data[symbol_name] = new annotation(filename);
}

bool reader_annotations::read_annotations_folder(std::string folder_name, std::map<std::string, annotation *> &data) {
	DIR *dir;
        struct dirent *ent;
        if (dir = opendir(folder_name.c_str())) {
		while ((ent = readdir (dir)) != NULL) {
			//if (strncmp(ent->d_name, "idcm", strlen("idcm")) == 0)
				read_annotations_file(folder_name + ent->d_name, "todo_implement_me", data);
		}
		closedir(dir);
	} else {
		perror ("Could not open dir");
		return false;
        }
	std::cout << "Read " << data.size() << " annotations " << std::endl;
        return true;
}
