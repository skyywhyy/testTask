#include <program2/tcp_server.hpp>

#include "test_utils.hpp"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstdint>
#include <future>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

class RawClient {
public:
    explicit RawClient(std::uint16_t port)
    {
        descriptor_ = ::socket(AF_INET, SOCK_STREAM, 0);
        if (descriptor_ == -1) {
            throw std::runtime_error{"failed to create client socket"};
        }

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(port);
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

        if (::connect(
                descriptor_,
                reinterpret_cast<const sockaddr*>(&address),
                sizeof(address)) == -1) {
            close();
            throw std::runtime_error{"failed to connect client socket"};
        }
    }

    ~RawClient()
    {
        close();
    }

    RawClient(const RawClient&) = delete;
    RawClient& operator=(const RawClient&) = delete;

    void send_all(std::string_view data) const
    {
        std::size_t sent = 0;
        while (sent < data.size()) {
            const auto result = ::send(
                descriptor_, data.data() + sent, data.size() - sent, 0);
            if (result <= 0) {
                throw std::runtime_error{"failed to send client data"};
            }
            sent += static_cast<std::size_t>(result);
        }
    }

    void close()
    {
        if (descriptor_ != -1) {
            ::close(descriptor_);
            descriptor_ = -1;
        }
    }

private:
    int descriptor_{-1};
};

class StartedServer {
public:
    StartedServer()
        : server_{0}
    {
        if (!server_.start()) {
            throw std::runtime_error{"failed to start server"};
        }
    }

    program2::TcpServer server_;
};

void expect_line(
    const std::optional<std::string>& actual,
    std::string_view expected,
    std::string_view test_name)
{
    test_utils::expect_true(actual.has_value(), test_name);
    if (actual.has_value()) {
        test_utils::expect_equal(*actual, std::string{expected}, test_name);
    }
}

void test_port_zero_binds_an_ephemeral_port()
{
    StartedServer fixture;

    test_utils::expect_true(
        fixture.server_.bound_port() != 0,
        "port zero binds an ephemeral loopback port");
}

void test_receives_one_line_without_delimiter()
{
    StartedServer fixture;
    RawClient client{fixture.server_.bound_port()};
    test_utils::expect_true(
        fixture.server_.accept_client(), "accepts a loopback client");

    client.send_all("128\n");

    expect_line(
        fixture.server_.receive_line(),
        "128",
        "one line is returned without newline");
}

void test_buffers_two_lines_from_one_send()
{
    StartedServer fixture;
    RawClient client{fixture.server_.bound_port()};
    test_utils::expect_true(
        fixture.server_.accept_client(), "accepts client for buffered lines");

    client.send_all("first\nsecond\n");

    expect_line(
        fixture.server_.receive_line(),
        "first",
        "first buffered line is returned");
    expect_line(
        fixture.server_.receive_line(),
        "second",
        "second buffered line is preserved");
}

void test_receive_line_blocks_until_split_line_is_complete()
{
    using namespace std::chrono_literals;

    StartedServer fixture;
    RawClient client{fixture.server_.bound_port()};
    test_utils::expect_true(
        fixture.server_.accept_client(), "accepts client for split send");

    client.send_all("12");
    auto pending_line = std::async(std::launch::async, [&fixture] {
        return fixture.server_.receive_line();
    });

    test_utils::expect_true(
        pending_line.wait_for(100ms) == std::future_status::timeout,
        "receive_line blocks on an incomplete line");

    client.send_all("8\n");
    test_utils::expect_true(
        pending_line.wait_for(2s) == std::future_status::ready,
        "receive_line completes after newline arrives");
    expect_line(
        pending_line.get(), "128", "split sends are combined into one line");
}

void test_disconnect_drops_partial_line_and_allows_second_client()
{
    StartedServer fixture;
    {
        RawClient first_client{fixture.server_.bound_port()};
        test_utils::expect_true(
            fixture.server_.accept_client(), "accepts first client");
        first_client.send_all("partial");
    }

    test_utils::expect_false(
        fixture.server_.receive_line().has_value(),
        "disconnect returns no partial line");
    fixture.server_.disconnect_client();
    fixture.server_.disconnect_client();

    RawClient second_client{fixture.server_.bound_port()};
    test_utils::expect_true(
        fixture.server_.accept_client(), "accepts second client");
    second_client.send_all("fresh\n");

    expect_line(
        fixture.server_.receive_line(),
        "fresh",
        "partial data is not carried into second client");

    fixture.server_.stop();
    fixture.server_.stop();
}

}  // namespace

int main()
{
    test_port_zero_binds_an_ephemeral_port();
    test_receives_one_line_without_delimiter();
    test_buffers_two_lines_from_one_send();
    test_receive_line_blocks_until_split_line_is_complete();
    test_disconnect_drops_partial_line_and_allows_second_client();

    return test_utils::finish();
}
