#include <iostream>
#include <memory>
#include <chrono>

#include "etcd/Client.hpp"
#include "etcd/KeepAlive.hpp"
#include "etcd/Watcher.hpp"

using namespace std::chrono_literals;

std::string ELECTIONKEY = "MyApp/leader";

class MyApp
{
    public:
        MyApp(const char* etcdConnectionString, std::string id)
        : m_id(std::move(id))
        , m_leaderId("")
        {
            m_etcd = std::make_shared<etcd::Client>(etcdConnectionString);
        }

        ~MyApp()
        {
            // we should cancel the watcher first so that deletion which happens
            // as part of keepalive cancel does not trigger the watcher callback
            if (m_watcher.get() != nullptr)
            {
                m_watcher->Cancel();
            }

            if (m_keepalive.get() != nullptr)
            {
                m_keepalive->Cancel();
            }
        }

        bool isLeader()
        {
            return m_id == m_leaderId;
        }

        std::string& GetID()
        {
            return m_id;
        }

        std::shared_ptr<etcd::KeepAlive> GetKeepAlive()
        {
            if (m_keepalive.get() == nullptr)
            {
                m_keepalive = m_etcd->leasekeepalive(10).get();
            }
            return m_keepalive;
        }

        void StartElection()
        {
            int numberOfTries = 10;
            while (numberOfTries--)
            {
                pplx::task<etcd::Response> response_task = m_etcd->add(ELECTIONKEY, GetID(), GetKeepAlive()->Lease());
                try
                {
                    etcd::Response response = response_task.get();

                    if (response.is_ok())
                    {
                        // if able to add the key that means this is the new leader
                        std::cout << "I am the leader" << std::endl;
                    }
                    
                    // capture the current leader (stored as value of the ELECTIONKEY)
                    // in case of is_ok() -> False, this returns the existing key's value
                    m_leaderId = response.value().as_string();
                    std::cout << "Rx leader: " << m_leaderId << std::endl;
                    break;
                }
                catch (std::exception const& ex)
                {
                    std::cerr << ex.what();
                    if (numberOfTries == 0)
                        throw ex;
                }
            }
        }

        void WatchForLeaderChange()
        {
            auto callback = [&](etcd::Response response) { this->WatchForLeaderChangeCallback(response); };
            m_watcher = std::make_unique<etcd::Watcher>(*m_etcd, ELECTIONKEY, callback);
        }

    private:
        void WatchForLeaderChangeCallback(etcd::Response response)
        {
            if (response.action() == "delete")
            {
                m_leaderId = "";
                StartElection();
            }
            else if (response.action() == "set")
            {
                m_leaderId = response.value().as_string();
            }
        }

        std::string m_id;
        std::string m_leaderId;
        std::shared_ptr<etcd::Client> m_etcd;
        std::shared_ptr<etcd::KeepAlive> m_keepalive;
        std::unique_ptr<etcd::Watcher> m_watcher;
};

int main(int argc, char ** argv)
{
    if (argc < 2)
    {
        throw "Not enough argument";
    }

    auto appId = std::string(argv[1]);
    MyApp app("127.0.0.1:2379", appId);

    app.StartElection();
    app.WatchForLeaderChange();

    std::this_thread::sleep_for(120s);
    return 0;
}

// g++ -std=c++14 -I /usr/local/include /usr/local/lib/libetcd-cpp-api.dylib main.cpp -o main