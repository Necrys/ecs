#pragma once

namespace ecs {

  struct events{};
  class registry;

  class event_handler_base {
  public:
    virtual ~event_handler_base() = default;
    virtual void handle_all() = 0;
  };

  template< typename E >
  struct system_event_handler_callback {
    std::function< void( const E& ) > f;
    size_t                            sid;
  };

  template < typename E >
  class event_handler: public event_handler_base {
  public:
    event_handler( registry& r );

    template < typename F >
    void subscribe( F&& f, const size_t sid );

    template < typename... Args >
    void enqueue( Args&& ...args );

    void handle_all() override;

  private:
    std::vector< system_event_handler_callback< E > > m_callbacks;
    std::vector< E > m_events[ 2 ];
    size_t           m_current;
    registry&        m_registry;
  };

}

#include "events.hpp"
