#include <arpa/inet.h>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <json-c/json.h>
#include <sstream>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <thread>
#include <unistd.h>
#include <vector>

#define BUFFER_SIZE 4096
#define TIMEOUT_SEC 5

// Structure to hold statistics
struct Statistics {
  int total_requests;
  int received_responses;
  double average_time;
  int mismatch_count;
};

// Function to print usage
void print_usage() {
  std::cerr << "Usage: ./tcp-json-rpc-client --servertcpport=<port> "
               "--requests=<requests.txt> --responses=<responses.txt> "
               "--repeat=<count>\n";
}

// Function to output statistics as JSON
void output_json_stats(const Statistics &stats) {
  json_object *root = json_object_new_object();
  json_object_object_add(root, "sent",
                         json_object_new_int(stats.total_requests));
  json_object_object_add(root, "received",
                         json_object_new_int(stats.received_responses));
  json_object_object_add(root, "averagemicrosec",
                         json_object_new_double(stats.average_time));
  json_object_object_add(root, "mismatch",
                         json_object_new_int(stats.mismatch_count));

  std::cout << json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN)
            << std::endl;
  json_object_put(root);
}
// Function to output statistics as text
void output_stats(const Statistics &stats) {
  std::cout << "sent: " << stats.total_requests << std::endl;
  std::cout << "received: " << stats.received_responses << std::endl;
  std::cout << "averagemicrosec: " << stats.average_time << std::endl;
  std::cout << "mismatch: " << stats.mismatch_count << std::endl;
}
// Function to parse command line arguments
bool parse_arguments(int argc, char *argv[], int &server_port,
                     std::string &requests_file, std::string &responses_file,
                     int &repeat_count) {
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg.find("--servertcpport=") == 0) {
      server_port = std::stoi(arg.substr(16));
    } else if (arg.find("--requests=") == 0) {
      requests_file = arg.substr(11);
    } else if (arg.find("--responses=") == 0) {
      responses_file = arg.substr(12);
    } else if (arg.find("--repeat=") == 0) {
      repeat_count = std::stoi(arg.substr(9));
    } else {
      std::cerr << "Unknown argument: " << arg << "\n";
      return false;
    }
  }
  return true;
}

// Function to read JSON requests from a file
std::vector<std::string> read_json_requests(const std::string &filename) {
  std::vector<std::string> requests;
  std::ifstream file(filename);
  std::string line;
  while (std::getline(file, line)) {
    requests.push_back(line);
  }
  return requests;
}

// Function to read JSON responses from a file
std::vector<std::string> read_json_responses(const std::string &filename) {
  std::vector<std::string> responses;
  std::ifstream file(filename);
  std::string line;
  while (std::getline(file, line)) {
    responses.push_back(line);
  }
  return responses;
}

// Function to modify the request JSON with a sequential ID
std::string modify_request_with_id(const std::string &request, int id) {
  json_object *request_obj = json_tokener_parse(request.c_str());
  json_object_object_add(request_obj, "id", json_object_new_int(id));
  const char *modified_request = json_object_to_json_string(request_obj);
  std::string result(modified_request);
  json_object_put(request_obj);
  return result;
}

// Function to compare two JSON objects (ignoring the "id" field)
bool compare_json_objects(const std::string &json1, const std::string &json2) {
  json_object *obj1 = json_tokener_parse(json1.c_str());
  json_object *obj2 = json_tokener_parse(json2.c_str());

  if (!obj1 || !obj2) {
    return false;
  }

  // Remove the "id" field before comparison
  json_object_object_del(obj1, "id");
  json_object_object_del(obj2, "id");

  bool result = json_object_equal(obj1, obj2);
  json_object_put(obj1);
  json_object_put(obj2);
  return result;
}

// Function to send JSON requests and receive responses
Statistics send_and_receive(int sock, const std::vector<std::string> &requests,
                            const std::vector<std::string> &responses,
                            int repeat_count) {
  std::vector<std::chrono::microseconds> response_times;
  Statistics stats = {0, 0, 0.0, 0};
  stats.total_requests = requests.size() * repeat_count;
  int current_id = 0;

  for (int repeat_index = 0; repeat_index < repeat_count; ++repeat_index) {
    for (size_t request_index = 0; request_index < requests.size();
         ++request_index) {
      std::string modified_request =
          modify_request_with_id(requests[request_index], current_id++);

      auto start_time = std::chrono::high_resolution_clock::now();

      if (send(sock, modified_request.c_str(), modified_request.size(), 0) <
          0) {
        std::cerr << "Failed to send request\n";
        return stats;
      }

      char buffer[BUFFER_SIZE] = {0};
      fd_set read_fds;
      FD_ZERO(&read_fds);
      FD_SET(sock, &read_fds);

      struct timeval timeout;
      timeout.tv_sec = TIMEOUT_SEC;
      timeout.tv_usec = 0;

      if (select(sock + 1, &read_fds, nullptr, nullptr, &timeout) > 0) {
        int bytes_received = recv(sock, buffer, BUFFER_SIZE, 0);
        if (bytes_received > 0) {
          auto end_time = std::chrono::high_resolution_clock::now();
          auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
              end_time - start_time);
          response_times.push_back(duration);

          std::string response(buffer, bytes_received);
          stats.received_responses++;

          if (!compare_json_objects(response, responses[request_index])) {
            stats.mismatch_count++;
          }
        }
      } else {
        std::cerr << "Timeout waiting for response\n";
        return stats;
      }
    }
  }

  // Calculate average response time
  long long total_time = 0;
  for (const auto &time : response_times) {
    total_time += time.count();
  }
  stats.average_time =
      (stats.total_requests > 0)
          ? static_cast<double>(total_time) / stats.total_requests
          : 0.0;

  return stats;
}

int main(int argc, char *argv[]) {
  int server_port;
  std::string requests_file, responses_file;
  int repeat_count;

  if (!parse_arguments(argc, argv, server_port, requests_file, responses_file,
                       repeat_count)) {
    print_usage();
    return 1;
  }

  auto requests = read_json_requests(requests_file);
  auto responses = read_json_responses(responses_file);

  if (requests.empty() || responses.empty()) {
    std::cerr << "No requests or responses found in the files\n";
    return 1;
  }

  if (requests.size() != responses.size()) {
    std::cerr << "Error: The number of requests and responses does not match\n";
    return 1;
  }

  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    std::cerr << "Failed to create socket\n";
    return 1;
  }

  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(server_port);
  if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0) {
    std::cerr << "Invalid address/ Address not supported\n";
    close(sock);
    return 1;
  }

  if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
    std::cerr << "Connection failed\n";
    close(sock);
    return 1;
  }

  // Get statistics from send_and_receive
  Statistics stats = send_and_receive(sock, requests, responses, repeat_count);

  // Output JSON formatted statistics
  // output_json_stats(stats);

  // Output JSON formatted statistics
  output_stats(stats);

  // Close the socket
  close(sock);

  // Return based on mismatch count
  return (stats.mismatch_count == 0) ? 0 : 1;
}
