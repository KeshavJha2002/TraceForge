#include <fstream>

static std::ofstream __trace_file("state_trace.log");

inline void __trace_event(const char *event_name, const char *label) {
  __trace_file << "[block] " << event_name << ":" << label << "\n";
}

template <typename T>
T __trace_return(const char *function_name, const T &value) {
  __trace_file << "[return] " << function_name << " value=" << value << "\n";
  return value;
}

template <typename T>
void __trace_state(const char *name, const T &value) {
  __trace_file << "[state] " << name << "=" << value << "\n";
}

#include <fstream>

#define base 10

int func(int a) {
  __trace_event("enter", "compound");
  __trace_file << "[enter] func a=" << a << "\n";
  if(a <= 1) {
    return __trace_return("func", a);
  }
  return __trace_return("func", a*base+func(a-1));

__trace_event("exit", "compound");}

int main() {
  __trace_event("enter", "compound");
  return __trace_return("main", func(5));

__trace_event("exit", "compound");}