#ifndef HOTEL_RESERVATION_H
#define HOTEL_RESERVATION_H

#include "hotel/reservation.h"
#include "hotel/persistentobject.h"

#include <boost/date_time.hpp>

#include <vector>
#include <memory>
#include <string>

namespace hotel {

class ReservationAtom;

/**
 * @brief The Reservation class represents a single reservation over a given date period
 *
 * One reservation supports changes of room, during the period of stay. To represent this, a Reservation is composed
 * of one or more ReservationAtom.
 *
 * This class does not store extensive information about the reservation, like owner and prices. This class is
 * primarely meant for display.
 *
 * More detailed information is stored in the DetailedReservation class.
 *
 * @see ReservationAtom
 * @see DetailedReservation
 */
class Reservation : public PersistentObject
{
public:
  Reservation(const std::string& description, int roomId, boost::gregorian::date_period dateRange);

  ReservationAtom* addContinuation(int room, boost::gregorian::date date);

  const std::vector<std::unique_ptr<ReservationAtom>>& atoms() const { return _atoms; }
  const int length() const;

public: // TODO: Public for now...
  std::string _description;
  std::vector<std::unique_ptr<ReservationAtom>> _atoms;
};

/**
 * @brief The ReservationAtom class represents one single reserved room over a given date period
 *
 * Objects of this class are owned by a Reservation. Each Atom keeps a back pointer to the parent reservation.
 */
class ReservationAtom : public PersistentObject
{
public:
  ReservationAtom(Reservation* reservation, const int room, boost::gregorian::date_period dateRange);

  bool isFirst() const { return this == _reservation->atoms().begin()->get(); }
  bool isLast() const { return this == _reservation->atoms().rbegin()->get(); }

public: // TODO: Public for now...
  Reservation* _reservation;
  int _roomId;
  boost::gregorian::date_period _dateRange;
};

} // namespace hotel

#endif // HOTEL_RESERVATION_H
