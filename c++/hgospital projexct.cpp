#include <iostream>
#include <queue>
#include <vector>
#include <random>
#include <chrono>
#include <iomanip>
#include <algorithm>

// Patient structure
struct Patient {
    int id;
    int arrivalTime;  // in minutes from start
    int severity;     // 1-5 (higher = more urgent)
    int treatmentTime; // estimated minutes
    int waitTime;     // time spent waiting
    int startTreatmentTime; // when treatment starts
};

// Comparator for priority queue (higher severity first, then earlier arrival)
struct PatientComparator {
    bool operator()(const Patient& a, const Patient& b) {
        if (a.severity != b.severity) return a.severity < b.severity;
        return a.arrivalTime > b.arrivalTime;
    }
};

// Resource structure (e.g., Doctor or Bed)
struct Resource {
    bool available;
    int endTime;  // when it becomes free again
};

// Simple Poisson-like arrival generator (lambda = avg arrivals per hour)
int generateArrivalTime(int currentTime, double lambdaPerHour, std::mt19937& rng) {
    std::exponential_distribution<double> expDist(1.0 / lambdaPerHour);
    double hoursToNext = expDist(rng);
    return currentTime + static_cast<int>(hoursToNext * 60);  // convert to minutes
}

// Generate random severity (1-5, biased toward higher for realism)
int generateSeverity(std::mt19937& rng) {
    std::uniform_int_distribution<int> dist(1, 10);
    int roll = dist(rng);
    return (roll >= 7) ? 5 : (roll >= 5) ? 4 : (roll >= 3) ? 3 : (roll >= 2) ? 2 : 1;
}

// Generate random treatment time (30-120 min, based on severity)
int generateTreatmentTime(int severity, std::mt19937& rng) {
    std::uniform_int_distribution<int> dist(30 + (severity * 10), 120);
    return dist(rng);
}

// "AI" predictor: Estimate resource needs based on time-of-day (e.g., peak hours)
double predictArrivalRate(int currentHour) {
    // Simple rule: Higher in evenings (e.g., 6-10 PM)
    if (currentHour >= 18 && currentHour <= 22) return 8.0;  // peak
    if (currentHour >= 0 && currentHour <= 6) return 2.0;    // night low
    return 5.0;  // average
}

int main() {
    std::random_device rd;
    std::mt19937 rng(rd());

    const int SIMULATION_MINUTES = 1440;  // 24 hours
    const int NUM_DOCTORS = 5;
    const int NUM_BEDS = 10;
    const double BASE_LAMBDA = 5.0;  // base arrivals per hour

    // Resources
    std::vector<Resource> doctors(NUM_DOCTORS);
    std::vector<Resource> beds(NUM_BEDS);
    for (auto& doc : doctors) { doc.available = true; doc.endTime = 0; }
    for (auto& bed : beds) { bed.available = true; bed.endTime = 0; }

    // Priority queue for waiting patients
    std::priority_queue<Patient, std::vector<Patient>, PatientComparator> waitingRoom;

    // Track patients
    std::vector<Patient> allPatients;
    int nextPatientId = 1;
    int currentTime = 0;
    int nextArrivalTime = generateArrivalTime(0, BASE_LAMBDA, rng);

    // Metrics
    double totalWaitTime = 0.0;
    int totalPatients = 0;
    int patientsTreated = 0;
    double doctorUtilization = 0.0;
    double bedUtilization = 0.0;

    while (currentTime < SIMULATION_MINUTES || !waitingRoom.empty()) {
        // Generate new arrivals (use predictor for lambda)
        int currentHour = currentTime / 60;
        double lambda = BASE_LAMBDA * predictArrivalRate(currentHour);
        while (nextArrivalTime <= currentTime && nextArrivalTime < SIMULATION_MINUTES) {
            Patient p;
            p.id = nextPatientId++;
            p.arrivalTime = nextArrivalTime;
            p.severity = generateSeverity(rng);
            p.treatmentTime = generateTreatmentTime(p.severity, rng);
            p.waitTime = 0;
            p.startTreatmentTime = 0;
            waitingRoom.push(p);
            allPatients.push_back(p);  // Track for metrics
            totalPatients++;
            nextArrivalTime = generateArrivalTime(nextArrivalTime, lambda, rng);
        }

        // Advance time to next event (simplified: process in 1-min increments, but optimize by jumping)
        int nextEventTime = SIMULATION_MINUTES;
        if (!waitingRoom.empty()) {
            nextEventTime = std::min(nextEventTime, waitingRoom.top().arrivalTime);
        }
        // Check resource availability
        for (auto& doc : doctors) {
            if (!doc.available && doc.endTime <= currentTime) doc.available = true;
            nextEventTime = std::min(nextEventTime, doc.endTime);
        }
        for (auto& bed : beds) {
            if (!bed.available && bed.endTime <= currentTime) bed.available = true;
            nextEventTime = std::min(nextEventTime, bed.endTime);
        }
        currentTime = std::min(currentTime + 1, nextEventTime);  // Step by 1 min or to event

        // Assign resources to highest priority patient if possible
        if (!waitingRoom.empty()) {
            Patient& topPatient = const_cast<Patient&>(waitingRoom.top());  // Non-const ref for update
            int assignTime = std::max(currentTime, topPatient.arrivalTime);

            // Find available doctor
            auto availDoctor = std::find_if(doctors.begin(), doctors.end(), [&](const Resource& r) {
                return r.available && assignTime >= r.endTime;
            });
            if (availDoctor != doctors.end()) {
                // Find available bed
                auto availBed = std::find_if(beds.begin(), beds.end(), [&](const Resource& r) {
                    return r.available && assignTime >= r.endTime;
                });
                if (availBed != beds.end()) {
                    // Assign
                    topPatient.waitTime = assignTime - topPatient.arrivalTime;
                    topPatient.startTreatmentTime = assignTime;
                    totalWaitTime += topPatient.waitTime;

                    availDoctor->available = false;
                    availDoctor->endTime = assignTime + topPatient.treatmentTime;
                    availBed->available = false;
                    availBed->endTime = assignTime + topPatient.treatmentTime;

                    waitingRoom.pop();
                    patientsTreated++;
                    doctorUtilization += topPatient.treatmentTime;
                    bedUtilization += topPatient.treatmentTime;
                }
            }
        }
    }

    // Calculate utilizations (as fraction of total time)
    doctorUtilization = (NUM_DOCTORS > 0) ? doctorUtilization / (NUM_DOCTORS * SIMULATION_MINUTES) : 0;
    bedUtilization = (NUM_BEDS > 0) ? bedUtilization / (NUM_BEDS * SIMULATION_MINUTES) : 0;
    double avgWaitTime = (patientsTreated > 0) ? totalWaitTime / patientsTreated : 0;

    // Output results
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "=== Hospital Simulation Results ===" << std::endl;
    std::cout << "Total Patients Arrived: " << totalPatients << std::endl;
    std::cout << "Patients Treated: " << patientsTreated << std::endl;
    std::cout << "Average Wait Time (minutes): " << avgWaitTime << std::endl;
    std::cout << "Doctor Utilization: " << (doctorUtilization * 100) << "%" << std::endl;
    std::cout << "Bed Utilization: " << (bedUtilization * 100) << "%" << std::endl;
    std::cout << "Note: This is a basic greedy allocation. For AI optimization, integrate with solvers like Gurobi or ML for predictions." << std::endl;

    return 0;
}
