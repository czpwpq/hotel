#include "gtest/gtest.h"

#include "persistence/datasource.h"
#include "persistence/op/operations.h"

#include "hotel/hotelcollection.h"

#include <condition_variable>


void waitForAllOperations(persistence::DataSource& ds)
{
  std::mutex mutex;
  std::unique_lock<std::mutex> lock(mutex);
  std::condition_variable condition;

  while(ds.hasPendingTasks() || ds.hasUninitializedStreams())
  {
    ds.processIntegrationQueue();
    condition.wait_for(lock, std::chrono::milliseconds(10));
  }

  ds.taskCompletedSignal().disconnect_all_slots();
}

void waitForTask(persistence::DataSource& ds, persistence::op::Task<persistence::op::OperationResults>& task)
{
  // Waits for one task to complete and to be integrated
  task.waitForCompletion();
  ds.processIntegrationQueue();
}


class Persistence : public testing::Test
{
public:
  void SetUp() override
  {
    // Make sure the database is empty before each test
    persistence::DataSource dataSource("test.db");
    auto task = dataSource.queueOperation(persistence::op::EraseAllData());
    waitForTask(dataSource, task);
  }

  hotel::Hotel makeNewHotel(const std::string& name, const std::string& category, int numberOfRooms)
  {
    hotel::Hotel hotel(name);
    hotel.addRoomCategory(std::make_unique<hotel::RoomCategory>(category, category));
    for (int i = 0; i < numberOfRooms; ++i)
      hotel.addRoom(std::make_unique<hotel::HotelRoom>("Room " + std::to_string(i + 1)), category);
    return hotel;
  }

  hotel::Reservation makeNewReservation(const std::string& description, int roomId)
  {
    using namespace boost::gregorian;
    date_period dateRange(date(2017, 1, 1), date(2017, 1, 1) + days(10));
    auto reservation = hotel::Reservation(description, roomId, dateRange);
    reservation.setStatus(hotel::Reservation::New);
    return reservation;
  }

  void storeHotel(persistence::DataSource& dataSource, const hotel::Hotel& hotel)
  {
    // TODO: Right now this test function only works for storing one instance
    auto task = dataSource.queueOperation(persistence::op::StoreNewHotel { std::make_unique<hotel::Hotel>(hotel) });
    waitForTask(dataSource, task);
  }

  void storeReservation(persistence::DataSource& dataSource, const hotel::Reservation& reservation)
  {
    // TODO: Right now this test function only works for storing one instance
    auto task = dataSource.queueOperation(persistence::op::StoreNewReservation{ std::make_unique<hotel::Reservation>(reservation) });
    waitForTask(dataSource, task);
  }
};

TEST_F(Persistence, HotelPersistence)
{
  auto hotel = makeNewHotel("Hotel 1", "Category 1", 10);
  auto hotelId = 0;
  // Store hotel
  {
    persistence::DataSource dataSource("test.db");
    persistence::VectorDataStreamObserver<hotel::Hotel> hotels;
    auto hotelsStreamHandle = dataSource.connectToStream(&hotels);
    storeHotel(dataSource, hotel);

    ASSERT_EQ(1u, hotels.items().size());
    ASSERT_EQ(hotel, hotels.items()[0]);
    hotelId = hotels.items()[0].id();
    ASSERT_NE(0, hotelId);
  }

  // Check data after reopening the database
  {
    persistence::DataSource dataSource("test.db");
    persistence::VectorDataStreamObserver<hotel::Hotel> hotels;
    auto hotelsStreamHandle = dataSource.connectToStream(&hotels);
    waitForAllOperations(dataSource);

    ASSERT_EQ(1u, hotels.items().size());
    ASSERT_EQ(hotel, hotels.items()[0]);
    ASSERT_EQ(hotelId, hotels.items()[0].id());
  }
}

TEST_F(Persistence, ReservationPersistence)
{
  auto hotel = makeNewHotel("Hotel 1", "Category 1", 10);
  hotel::Reservation reservation("");
  auto reservationId = 0;
  // Store reservation
  {
    persistence::DataSource dataSource("test.db");
    persistence::VectorDataStreamObserver<hotel::Hotel> hotels;
    auto hotelsStreamHandle = dataSource.connectToStream(&hotels);
    storeHotel(dataSource, hotel);

    ASSERT_EQ(1u, hotels.items().size());
    auto roomId = hotels.items()[0].rooms()[0]->id();

    persistence::VectorDataStreamObserver<hotel::Reservation> reservations;
    auto reservationsStreamHandle = dataSource.connectToStream(&reservations);
    reservation = makeNewReservation("", roomId);
    storeReservation(dataSource, reservation);

    reservationId = reservations.items()[0].id();

    ASSERT_EQ(reservation, reservations.items()[0]);
    ASSERT_NE(0, reservationId);
  }

  // Check data after reopening the database
  {
    persistence::DataSource dataSource("test.db");
    persistence::VectorDataStreamObserver<hotel::Reservation> reservations;
    auto reservationsStreamHandle = dataSource.connectToStream(&reservations);
    waitForAllOperations(dataSource);

    ASSERT_EQ(1u, reservations.items().size());
    ASSERT_EQ(reservation, reservations.items()[0]);
  }
}

TEST_F(Persistence, DataStreams)
{
  persistence::DataSource dataSource("test.db");

  persistence::VectorDataStreamObserver<hotel::Hotel> hotels;
  persistence::VectorDataStreamObserver<hotel::Reservation> reservations;
  auto hotelsStreamHandle = dataSource.connectToStream(&hotels);
  auto reservationsStreamHandle = dataSource.connectToStream(&reservations);

  ASSERT_EQ(0u, hotels.items().size());
  ASSERT_EQ(0u, reservations.items().size());

  storeHotel(dataSource, makeNewHotel("Hotel 1", "Category 1", 10));

  ASSERT_EQ(1u, hotels.items().size());
  ASSERT_EQ(0u, reservations.items().size());

  storeReservation(dataSource, makeNewReservation("", hotels.items()[0].rooms()[0]->id()));
  storeReservation(dataSource, makeNewReservation("", hotels.items()[0].rooms()[1]->id()));

  ASSERT_EQ(1u, hotels.items().size());
  ASSERT_EQ(2u, reservations.items().size());

  auto task = dataSource.queueOperation(persistence::op::EraseAllData());
  waitForTask(dataSource, task);

  ASSERT_EQ(0u, hotels.items().size());
  ASSERT_EQ(0u, reservations.items().size());
}

TEST_F(Persistence, DataStreamsServices)
{
  persistence::DataSource dataSource("test.db");

  persistence::VectorDataStreamObserver<hotel::Hotel> hotels;
  auto hotelsStreamHandle = dataSource.connectToStream(&hotels);

  ASSERT_EQ(0u, hotels.items().size());

  storeHotel(dataSource, makeNewHotel("Hotel 1", "Category 1", 10));
  storeHotel(dataSource, makeNewHotel("Hotel 2", "Category 2", 11));

  ASSERT_EQ(2u, hotels.items().size());

//  persistence::VectorDataStreamObserver<hotel::Hotel> hotel;
//  nlohmann::json streamOptions;
//  streamOptions["id"] = hotels.items()[1].id();
//  auto hotelStreamHandle = dataSource.connectToStream(&hotel, "hotel.by_id", streamOptions);
//  waitForAllOperations(dataSource);

//  ASSERT_EQ(1u, hotel.items().size());
//  ASSERT_EQ(hotels.items()[1], hotel.items()[0]);

//  auto task = dataSource.queueOperation(persistence::op::EraseAllData());
//  waitForTask(dataSource, task);

//  ASSERT_EQ(0u, hotels.items().size());
//  ASSERT_EQ(0u, hotel.items().size());
}
