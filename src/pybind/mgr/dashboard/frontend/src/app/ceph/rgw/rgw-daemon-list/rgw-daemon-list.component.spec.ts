import { HttpClientTestingModule } from '@angular/common/http/testing';
import { ComponentFixture, TestBed } from '@angular/core/testing';
import { BrowserAnimationsModule } from '@angular/platform-browser/animations';
import { RouterTestingModule } from '@angular/router/testing';

import { TabsModule } from 'ngx-bootstrap/tabs';
import { of } from 'rxjs';

import { configureTestBed, i18nProviders } from '../../../../testing/unit-test-helper';
import { RgwSiteService } from '../../../shared/api/rgw-site.service';
import { Permissions } from '../../../shared/models/permissions';
import { AuthStorageService } from '../../../shared/services/auth-storage.service';
import { SharedModule } from '../../../shared/shared.module';
import { PerformanceCounterModule } from '../../performance-counter/performance-counter.module';
import { RgwDaemonDetailsComponent } from '../rgw-daemon-details/rgw-daemon-details.component';
import { RgwDaemonListComponent } from './rgw-daemon-list.component';

describe('RgwDaemonListComponent', () => {
  let component: RgwDaemonListComponent;
  let fixture: ComponentFixture<RgwDaemonListComponent>;
  let getPermissionsSpy: jasmine.Spy;
  let getRealmsSpy: jasmine.Spy;
  const permissions = new Permissions({ grafana: ['read'] });

  configureTestBed({
    declarations: [RgwDaemonListComponent, RgwDaemonDetailsComponent],
    imports: [
      BrowserAnimationsModule,
      HttpClientTestingModule,
      TabsModule.forRoot(),
      PerformanceCounterModule,
      SharedModule,
      RouterTestingModule
    ],
    providers: i18nProviders
  });

  beforeEach(() => {
    getPermissionsSpy = spyOn(TestBed.get(AuthStorageService), 'getPermissions');
    getPermissionsSpy.and.returnValue(new Permissions({}));
    getRealmsSpy = spyOn(TestBed.get(RgwSiteService), 'get');
    getRealmsSpy.and.returnValue(of([]));
    fixture = TestBed.createComponent(RgwDaemonListComponent);
    component = fixture.componentInstance;
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });

  it('should only show Daemons List tab', () => {
    fixture.detectChanges();

    const tabs = fixture.debugElement.nativeElement.querySelectorAll('tab');
    expect(tabs.length).toEqual(1);
    expect(tabs[0].getAttribute('heading')).toEqual('Daemons List');
  });

  it('should show Overall Performance tab', () => {
    getPermissionsSpy.and.returnValue(permissions);
    fixture.detectChanges();

    const tabs = fixture.debugElement.nativeElement.querySelectorAll('tab');
    expect(tabs.length).toEqual(2);
    expect(tabs[1].getAttribute('heading')).toEqual('Overall Performance');
  });

  it('should show Sync Performance tab', () => {
    getPermissionsSpy.and.returnValue(permissions);
    getRealmsSpy.and.returnValue(of(['realm1']));
    fixture.detectChanges();

    const tabs = fixture.debugElement.nativeElement.querySelectorAll('tab');
    expect(tabs.length).toEqual(3);
    expect(tabs[2].getAttribute('heading')).toEqual('Sync Performance');
  });
});
